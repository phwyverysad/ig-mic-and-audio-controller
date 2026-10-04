#include "hotkey_hook.h"
#include <string>
#include <cwctype>

static HHOOK g_kbdHook = NULL;
static HHOOK g_mouseHook = NULL;
static AppConfig g_config = GetDefaultConfig();
static RecordTarget g_recordTarget = RecordTarget::None;

static HotkeyHook::BroadcastCallback g_broadcastCb = nullptr;
static HotkeyHook::EmergencyExitCallback g_emergencyExitCb = nullptr;
static HotkeyHook::KeyRecordedCallback g_keyRecordedCb = nullptr;

static bool g_micActive = true;
static bool g_audioActive = true;
static bool g_micKeyHeld = false;
static bool g_audioKeyHeld = false;

static bool IsModifierKey(DWORD vk) {
    return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_MENU    || vk == VK_LMENU    || vk == VK_RMENU ||
           vk == VK_SHIFT   || vk == VK_LSHIFT   || vk == VK_RSHIFT ||
           vk == VK_LWIN    || vk == VK_RWIN;
}

bool HotkeyHook::Install(
    BroadcastCallback broadcastCb,
    EmergencyExitCallback emergencyExitCb,
    KeyRecordedCallback keyRecordedCb
) {
    g_broadcastCb = broadcastCb;
    g_emergencyExitCb = emergencyExitCb;
    g_keyRecordedCb = keyRecordedCb;

    if (g_kbdHook == NULL) {
        g_kbdHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandleW(NULL), 0);
    }
    if (g_mouseHook == NULL) {
        g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandleW(NULL), 0);
    }

    UpdateConfig(g_config);
    return (g_kbdHook != NULL) && (g_mouseHook != NULL);
}

void HotkeyHook::Uninstall() {
    if (g_kbdHook != NULL) {
        UnhookWindowsHookEx(g_kbdHook);
        g_kbdHook = NULL;
    }
    if (g_mouseHook != NULL) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = NULL;
    }
}

void HotkeyHook::UpdateConfig(const AppConfig& config) {
    g_config = config;
    // หากเป็นโหมด Push-to-Talk สถานะเริ่มต้นของไมค์ต้องเป็นปิด (Mute)
    if (g_config.micMode == 1) {
        g_micActive = false;
    } else {
        g_micActive = true;
    }
    g_audioActive = true;
    g_micKeyHeld = false;
    g_audioKeyHeld = false;
}

void HotkeyHook::StartRecording(RecordTarget target) {
    g_recordTarget = target;
}

void HotkeyHook::CancelRecording() {
    g_recordTarget = RecordTarget::None;
}

bool HotkeyHook::IsRecording() {
    return g_recordTarget != RecordTarget::None;
}

void HotkeyHook::SetMicActive(bool active) {
    g_micActive = active;
}

void HotkeyHook::SetAudioActive(bool active) {
    g_audioActive = active;
}

bool HotkeyHook::GetMicActive() {
    return g_micActive;
}

bool HotkeyHook::GetAudioActive() {
    return g_audioActive;
}

bool HotkeyHook::IsTargetWindowActive() {
    if (g_config.windowScope == 0) {
        return true; // ทำงานทุกหน้าต่าง (Global)
    }

    HWND hForeground = GetForegroundWindow();
    if (!hForeground) return false;

    // ตรวจสอบ Title ของหน้าต่าง
    wchar_t title[512] = { 0 };
    GetWindowTextW(hForeground, title, 512);
    std::wstring titleStr(title);
    for (auto& c : titleStr) c = (wchar_t)towlower(c);
    if (titleStr.find(L"instagram") != std::wstring::npos) {
        return true;
    }

    // ตรวจสอบชื่อ Process ของ Browser
    DWORD pid = 0;
    GetWindowThreadProcessId(hForeground, &pid);
    if (pid != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess) {
            wchar_t procPath[MAX_PATH] = { 0 };
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(hProcess, 0, procPath, &size)) {
                std::wstring procStr(procPath);
                for (auto& c : procStr) c = (wchar_t)towlower(c);
                if (procStr.find(L"chrome.exe") != std::wstring::npos ||
                    procStr.find(L"msedge.exe") != std::wstring::npos ||
                    procStr.find(L"brave.exe") != std::wstring::npos ||
                    procStr.find(L"firefox.exe") != std::wstring::npos ||
                    procStr.find(L"opera.exe") != std::wstring::npos ||
                    procStr.find(L"operagx.exe") != std::wstring::npos ||
                    procStr.find(L"vivaldi.exe") != std::wstring::npos ||
                    procStr.find(L"arc.exe") != std::wstring::npos ||
                    procStr.find(L"chromium.exe") != std::wstring::npos ||
                    procStr.find(L"zen.exe") != std::wstring::npos ||
                    procStr.find(L"waterfox.exe") != std::wstring::npos) {
                    CloseHandle(hProcess);
                    return true;
                }
            }
            CloseHandle(hProcess);
        }
    }

    return false;
}

bool HotkeyHook::MatchesBinding(const HotkeyBinding& binding, DWORD vk, bool isKeyDown) {
    if (binding.vkCode == 0 || binding.vkCode != vk) return false;

    // สำหรับ Key Down ตรวจสอบปุ่ม Modifier ครบถ้วน
    if (isKeyDown) {
        bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        bool winDown = ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);

        bool isCtrlKey = (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL);
        bool isAltKey = (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU);
        bool isShiftKey = (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT);
        bool isWinKey = (vk == VK_LWIN || vk == VK_RWIN);

        if (!isCtrlKey && ctrlDown != binding.ctrl) return false;
        if (!isAltKey && altDown != binding.alt) return false;
        if (!isShiftKey && shiftDown != binding.shift) return false;
        if (!isWinKey && winDown != binding.win) return false;
    }
    return true;
}

void HotkeyHook::HandleInput(DWORD vk, bool isKeyDown) {
    // โหมดบันทึกปุ่มเพื่อตั้งค่า
    if (g_recordTarget != RecordTarget::None) {
        if (isKeyDown) {
            if (IsModifierKey(vk)) {
                // หากเป็นปุ่ม Modifier เดี่ยวๆ กำลังกดค้าง ให้รอปุ่มหลักถัดไป
                return;
            }

            HotkeyBinding binding;
            binding.vkCode = vk;
            binding.ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            binding.alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            binding.shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            binding.win = ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);

            RecordTarget target = g_recordTarget;
            g_recordTarget = RecordTarget::None;
            if (g_keyRecordedCb) {
                g_keyRecordedCb(target, binding);
            }
        }
        return;
    }

    // ปุ่มฉุกเฉิน Ctrl + Alt + Q
    if (isKeyDown && vk == 'Q') {
        bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        if (ctrl && alt) {
            if (g_emergencyExitCb) g_emergencyExitCb();
            return;
        }
    }

    // ตรวจสอบเงื่อนไขหน้าต่างเป้าหมาย
    bool targetActive = IsTargetWindowActive();
    // หากหน้าต่างเป้าหมายไม่ได้แอคทีฟ และไม่มีปุ่มใดกำลังถูกกดค้างอยู่ ให้ข้ามการกดปุ่มใหม่
    if (!targetActive && !g_micKeyHeld && !g_audioKeyHeld) {
        return;
    }

    bool isMicKeyUp = (!isKeyDown && (vk == g_config.micHotkey.vkCode ||
        (g_config.micHotkey.ctrl && (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL)) ||
        (g_config.micHotkey.alt && (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU)) ||
        (g_config.micHotkey.shift && (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT)) ||
        (g_config.micHotkey.win && (vk == VK_LWIN || vk == VK_RWIN))));

    bool isAudioKeyUp = (!isKeyDown && (vk == g_config.audioHotkey.vkCode ||
        (g_config.audioHotkey.ctrl && (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL)) ||
        (g_config.audioHotkey.alt && (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU)) ||
        (g_config.audioHotkey.shift && (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT)) ||
        (g_config.audioHotkey.win && (vk == VK_LWIN || vk == VK_RWIN))));

    // 1. ตรวจสอบปุ่มไมโครโฟน
    if (g_config.micMode == 0) { // โหมด Toggle (สลับเปิด/ปิด)
        if (isKeyDown && MatchesBinding(g_config.micHotkey, vk, true)) {
            if (targetActive && !g_micKeyHeld) {
                g_micKeyHeld = true;
                g_micActive = !g_micActive;
                if (g_broadcastCb) {
                    std::string stateStr = g_micActive ? "true" : "false";
                    g_broadcastCb("{\"action\":\"set_mic\",\"state\":" + stateStr + "}");
                }
            }
        } else if (isMicKeyUp) {
            g_micKeyHeld = false;
        }
    } else { // โหมด Push-to-Talk (กดค้างเพื่อพูด)
        if (isKeyDown && MatchesBinding(g_config.micHotkey, vk, true)) {
            if (targetActive && !g_micKeyHeld) {
                g_micKeyHeld = true;
                g_micActive = true;
                if (g_broadcastCb) {
                    g_broadcastCb("{\"action\":\"set_mic\",\"state\":true}");
                }
            }
        } else if (isMicKeyUp) {
            if (g_micKeyHeld || g_micActive) {
                g_micKeyHeld = false;
                g_micActive = false;
                if (g_broadcastCb) {
                    g_broadcastCb("{\"action\":\"set_mic\",\"state\":false}");
                }
            }
        }
    }

    // 2. ตรวจสอบปุ่มหูฟังและเสียง
    if (g_config.audioMode == 0) { // โหมด Toggle (สลับเปิด/ปิด)
        if (isKeyDown && MatchesBinding(g_config.audioHotkey, vk, true)) {
            if (targetActive && !g_audioKeyHeld) {
                g_audioKeyHeld = true;
                g_audioActive = !g_audioActive;
                if (g_broadcastCb) {
                    std::string stateStr = g_audioActive ? "true" : "false";
                    g_broadcastCb("{\"action\":\"set_audio\",\"state\":" + stateStr + "}");
                }
            }
        } else if (isAudioKeyUp) {
            g_audioKeyHeld = false;
        }
    } else { // โหมด Push-to-Mute (กดค้างปิดเสียง)
        if (isKeyDown && MatchesBinding(g_config.audioHotkey, vk, true)) {
            if (targetActive && !g_audioKeyHeld) {
                g_audioKeyHeld = true;
                g_audioActive = false;
                if (g_broadcastCb) {
                    g_broadcastCb("{\"action\":\"set_audio\",\"state\":false}");
                }
            }
        } else if (isAudioKeyUp) {
            if (g_audioKeyHeld || !g_audioActive) {
                g_audioKeyHeld = false;
                g_audioActive = true;
                if (g_broadcastCb) {
                    g_broadcastCb("{\"action\":\"set_audio\",\"state\":true}");
                }
            }
        }
    }
}

LRESULT CALLBACK HotkeyHook::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKbd = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vk = pKbd->vkCode;
        bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

        if (isKeyDown || isKeyUp) {
            HandleInput(vk, isKeyDown);
            if (g_recordTarget != RecordTarget::None && isKeyDown && !IsModifierKey(vk)) {
                return 1; // กลืนปุ่มนี้ขณะกำลังบันทึกปุ่ม
            }
        }
    }
    return CallNextHookEx(g_kbdHook, nCode, wParam, lParam);
}

LRESULT CALLBACK HotkeyHook::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        DWORD vk = 0;
        bool isDown = false;
        bool isUp = false;

        switch (wParam) {
            case WM_LBUTTONDOWN: vk = VK_LBUTTON; isDown = true; break;
            case WM_LBUTTONUP:   vk = VK_LBUTTON; isUp = true;   break;
            case WM_RBUTTONDOWN: vk = VK_RBUTTON; isDown = true; break;
            case WM_RBUTTONUP:   vk = VK_RBUTTON; isUp = true;   break;
            case WM_MBUTTONDOWN: vk = VK_MBUTTON; isDown = true; break;
            case WM_MBUTTONUP:   vk = VK_MBUTTON; isUp = true;   break;
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP: {
                MSLLHOOKSTRUCT* pMouse = (MSLLHOOKSTRUCT*)lParam;
                WORD xBtn = HIWORD(pMouse->mouseData);
                if (xBtn == XBUTTON1) vk = VK_XBUTTON1;
                else if (xBtn == XBUTTON2) vk = VK_XBUTTON2;
                isDown = (wParam == WM_XBUTTONDOWN);
                isUp = (wParam == WM_XBUTTONUP);
                break;
            }
        }

        if (vk != 0 && (isDown || isUp)) {
            HandleInput(vk, isDown);
            if (g_recordTarget != RecordTarget::None && isDown) {
                return 1; // กลืนปุ่มเมาส์ขณะกำลังบันทึกปุ่ม
            }
        }
    }
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}
