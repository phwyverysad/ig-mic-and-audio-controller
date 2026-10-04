#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include "gui.h"
#include "hotkey_hook.h"
#include "ws_server.h"
#include "native_host.h"

#pragma comment(lib, "comctl32.lib")

WebSocketServer g_wsServer;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    AppConfig cfg = LoadConfig(GetIniFilePath());
    SetAutoStartRegistered(cfg.autoStartWithWeb);

    bool isNativeLaunch = IsNativeMessagingLaunch();

    // 1. Single Instance Guard ด้วย Named Mutex ป้องกันการเปิดโปรแกรมซ้ำซ้อน
    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"IGAudioController_SingleInstance_Mutex_v1");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (isNativeLaunch) {
            // ตอบกลับ Native Messaging จากเบราว์เซอร์ว่ากำลังทำงานอยู่แล้ว
            HandleNativeMessagingIO(true);
        } else {
            // หากผู้ใช้ดับเบิลคลิกเปิดโปรแกรมซ้ำเอง ให้ดึงหน้าต่างเดิมขึ้นมาแสดงแทน
            HWND hExisting = FindWindowW(L"IGAudioControllerGUI", NULL);
            if (hExisting) {
                ShowWindow(hExisting, SW_SHOW);
                ShowWindow(hExisting, SW_RESTORE);
                SetForegroundWindow(hExisting);
            }
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // ตอบกลับ Native Messaging สำหรับ Instance แรกที่เปิดขึ้นมา
    if (isNativeLaunch) {
        wchar_t exePath[MAX_PATH] = { 0 };
        GetModuleFileNameW(NULL, exePath, MAX_PATH);

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        CreateProcessW(
            exePath,
            NULL,
            NULL,
            NULL,
            FALSE,
            DETACHED_PROCESS | CREATE_BREAKAWAY_FROM_JOB,
            NULL,
            NULL,
            &si,
            &pi
        );
        if (pi.hProcess) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        HandleNativeMessagingIO(false);
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // 2. Initialize Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // 3. สร้างหน้าต่าง GUI
    MainWindow app(hInstance);
    if (!app.Initialize(nCmdShow)) {
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    HWND hWnd = app.GetHwnd();

    cfg = LoadConfig(GetIniFilePath());
    HotkeyHook::UpdateConfig(cfg);

    bool hookInstalled = HotkeyHook::Install(
        [](const std::string& msg) {
            g_wsServer.Broadcast(msg);
        },
        [hWnd]() {
            // ปิดโปรแกรมฉุกเฉินผ่าน Ctrl + Alt + Q
            PostMessageW(hWnd, WM_COMMAND, MAKEWPARAM(ID_TRAY_EXIT, 0), 0);
        },
        [hWnd](RecordTarget target, const HotkeyBinding& binding) {
            LPARAM lParam = (binding.vkCode & 0xFFFF) |
                            (binding.ctrl ? 0x10000 : 0) |
                            (binding.alt ? 0x20000 : 0) |
                            (binding.shift ? 0x40000 : 0) |
                            (binding.win ? 0x80000 : 0);
            PostMessageW(hWnd, WM_KEY_RECORDED, (WPARAM)target, lParam);
        }
    );

    // 5. เริ่มต้น WebSocket Server พร้อม Initial Sync และการซิงค์สถานะข้ามหน้าจอแบบสองทาง
    g_wsServer.Start(
        18888,
        [hWnd](int clientCount) {
            PostMessageW(hWnd, WM_CLIENT_COUNT_CHANGED, (WPARAM)clientCount, 0);
        },
        [&app](SOCKET clientSock) {
            const AppConfig& currentCfg = app.GetConfig();
            std::string syncMsg = "{\"action\":\"sync_state\","
                "\"mic_active\":" + std::string(HotkeyHook::GetMicActive() ? "true" : "false") + ","
                "\"audio_active\":" + std::string(HotkeyHook::GetAudioActive() ? "true" : "false") + ","
                "\"mic_mode\":" + std::to_string(currentCfg.micMode) + ","
                "\"audio_mode\":" + std::to_string(currentCfg.audioMode) + ","
                "\"beep_enabled\":" + (currentCfg.beepEnabled ? "true" : "false") + ","
                "\"auto_start_enabled\":" + (currentCfg.autoStartWithWeb ? "true" : "false") + "}";
            g_wsServer.SendTo(clientSock, syncMsg);
        },
        [&app](const std::string& msg, SOCKET sender) {
            if (msg.find("request_sync") != std::string::npos) {
                const AppConfig& currentCfg = app.GetConfig();
                std::string syncMsg = "{\"action\":\"sync_state\","
                    "\"mic_active\":" + std::string(HotkeyHook::GetMicActive() ? "true" : "false") + ","
                    "\"audio_active\":" + std::string(HotkeyHook::GetAudioActive() ? "true" : "false") + ","
                    "\"mic_mode\":" + std::to_string(currentCfg.micMode) + ","
                    "\"audio_mode\":" + std::to_string(currentCfg.audioMode) + ","
                    "\"beep_enabled\":" + (currentCfg.beepEnabled ? "true" : "false") + ","
                    "\"auto_start_enabled\":" + (currentCfg.autoStartWithWeb ? "true" : "false") + "}";
                g_wsServer.SendTo(sender, syncMsg);
                return;
            }

            if (msg.find("ui_state_change") != std::string::npos) {
                bool isMic = (msg.find("\"type\":\"mic\"") != std::string::npos || msg.find("\"type\": \"mic\"") != std::string::npos);
                bool isTrue = (msg.find("\"state\":true") != std::string::npos || msg.find("\"state\": true") != std::string::npos);

                if (isMic) {
                    HotkeyHook::SetMicActive(isTrue);
                } else {
                    HotkeyHook::SetAudioActive(isTrue);
                }

                g_wsServer.Broadcast(msg, sender);
            }
        }
    );

    // 6. Windows Message Loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 7. Cleanup เมื่อโปรแกรมปิด
    HotkeyHook::Uninstall();
    g_wsServer.Stop();

    if (hMutex) {
        CloseHandle(hMutex);
    }

    return (int)msg.wParam;
}
