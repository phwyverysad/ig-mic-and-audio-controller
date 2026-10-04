#include "gui.h"
#include "key_names.h"
#include "hotkey_hook.h"
#include "native_host.h"
#include "extension_installer.h"
#include <windowsx.h>
#include <sstream>

#define IDC_BTN_MIC_KEY 1001
#define IDC_CMB_MIC_MODE 1002
#define IDC_BTN_AUDIO_KEY 1003
#define IDC_CMB_AUDIO_MODE 1004
#define IDC_CMB_WINDOW_SCOPE 1005
#define IDC_CHK_BEEP 1006
#define IDC_CHK_AUTO_START 1007
#define IDC_BTN_SAVE_HIDE 1008
#define IDC_LBL_STATUS 1009
#define IDC_BTN_INSTALL_EXT 1010

extern WebSocketServer g_wsServer;

static MainWindow* g_pApp = nullptr;

MainWindow::MainWindow(HINSTANCE hInstance)
    : m_hInstance(hInstance),
      m_hWnd(NULL),
      m_hFont(NULL),
      m_connectedClients(0),
      m_recordingMic(false),
      m_recordingAudio(false),
      m_btnMicKey(NULL),
      m_cmbMicMode(NULL),
      m_btnAudioKey(NULL),
      m_cmbAudioMode(NULL),
      m_cmbWindowScope(NULL),
      m_chkBeep(NULL),
      m_chkAutoStart(NULL),
      m_lblStatus(NULL),
      m_btnInstallExt(NULL),
      m_btnSaveHide(NULL)
{
    g_pApp = this;
    ZeroMemory(&m_nid, sizeof(m_nid));
}

MainWindow::~MainWindow() {
    RemoveSystemTray();
    if (m_hFont) {
        DeleteObject(m_hFont);
    }
}

bool MainWindow::Initialize(int nCmdShow) {
    m_config = LoadConfig(GetIniFilePath());

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInstance;
    wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"IGAudioControllerGUI";

    RegisterClassExW(&wc);

    // ฟอนต์ Segoe UI รองรับภาษาไทยคมชัด
    m_hFont = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    int width = 540;
    int height = 580;
    int screenX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int screenY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    m_hWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"IGAudioControllerGUI",
        L"IG Audio Controller - การตั้งค่า",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        screenX, screenY, width, height,
        NULL, NULL, m_hInstance, NULL
    );

    if (!m_hWnd) return false;

    CreateControls();
    LoadConfigToUI();
    InitSystemTray();

    // ตรวจสอบ Extension หากยังไม่ได้ติดตั้ง ให้ดาวน์โหลดและติดตั้งอัตโนมัติ
    if (!ExtensionInstaller::IsExtensionFilesPresent()) {
        ExtensionInstaller::InstallAndOpenInBrowser(m_hWnd);
    }

    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);

    return true;
}

void MainWindow::CreateControls() {
    // 1. GroupBox ไมโครโฟน
    HWND grpMic = CreateWindowExW(0, L"BUTTON", L"การตั้งค่าไมโครโฟน",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 15, 485, 100, m_hWnd, NULL, m_hInstance, NULL);
    SendMessageW(grpMic, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    HWND lblMicKey = CreateWindowExW(0, L"STATIC", L"ปุ่มลัดไมค์:",
        WS_CHILD | WS_VISIBLE,
        35, 45, 80, 24, m_hWnd, NULL, m_hInstance, NULL);
    SendMessageW(lblMicKey, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    m_btnMicKey = CreateWindowExW(0, L"BUTTON", L"[ F8 ] คลิกเพื่อเปลี่ยน",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        120, 40, 210, 30, m_hWnd, (HMENU)IDC_BTN_MIC_KEY, m_hInstance, NULL);
    SendMessageW(m_btnMicKey, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    HWND lblMicMode = CreateWindowExW(0, L"STATIC", L"โหมด:",
        WS_CHILD | WS_VISIBLE,
        340, 45, 45, 24, m_hWnd, NULL, m_hInstance, NULL);
    SendMessageW(lblMicMode, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    m_cmbMicMode = CreateWindowExW(0, L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        390, 42, 100, 120, m_hWnd, (HMENU)IDC_CMB_MIC_MODE, m_hInstance, NULL);
    SendMessageW(m_cmbMicMode, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    ComboBox_AddString(m_cmbMicMode, L"สลับเปิด/ปิด");
    ComboBox_AddString(m_cmbMicMode, L"กดค้างเพื่อพูด");

    // 2. GroupBox หูฟังและเสียง
    HWND grpAudio = CreateWindowExW(0, L"BUTTON", L"การตั้งค่าหูฟังและเสียง",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 125, 485, 100, m_hWnd, NULL, m_hInstance, NULL);
    SendMessageW(grpAudio, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    HWND lblAudioKey = CreateWindowExW(0, L"STATIC", L"ปุ่มลัดหูฟัง:",
        WS_CHILD | WS_VISIBLE,
        35, 155, 80, 24, m_hWnd, NULL, m_hInstance, NULL);
    SendMessageW(lblAudioKey, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    m_btnAudioKey = CreateWindowExW(0, L"BUTTON", L"[ F9 ] คลิกเพื่อเปลี่ยน",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        120, 150, 210, 30, m_hWnd, (HMENU)IDC_BTN_AUDIO_KEY, m_hInstance, NULL);
    SendMessageW(m_btnAudioKey, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    HWND lblAudioMode = CreateWindowExW(0, L"STATIC", L"โหมด:",
        WS_CHILD | WS_VISIBLE,
        340, 155, 45, 24, m_hWnd, NULL, m_hInstance, NULL);
    SendMessageW(lblAudioMode, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    m_cmbAudioMode = CreateWindowExW(0, L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        390, 152, 100, 120, m_hWnd, (HMENU)IDC_CMB_AUDIO_MODE, m_hInstance, NULL);
    SendMessageW(m_cmbAudioMode, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    ComboBox_AddString(m_cmbAudioMode, L"สลับเปิด/ปิด");
    ComboBox_AddString(m_cmbAudioMode, L"กดค้างปิดเสียง");

    // 3. GroupBox การตั้งค่าทั่วไป
    HWND grpGeneral = CreateWindowExW(0, L"BUTTON", L"การตั้งค่าทั่วไป",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 235, 485, 135, m_hWnd, NULL, m_hInstance, NULL);
    SendMessageW(grpGeneral, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    HWND lblScope = CreateWindowExW(0, L"STATIC", L"ขอบเขตการทำงาน:",
        WS_CHILD | WS_VISIBLE,
        35, 262, 130, 24, m_hWnd, NULL, m_hInstance, NULL);
    SendMessageW(lblScope, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    m_cmbWindowScope = CreateWindowExW(0, L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        170, 258, 320, 120, m_hWnd, (HMENU)IDC_CMB_WINDOW_SCOPE, m_hInstance, NULL);
    SendMessageW(m_cmbWindowScope, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    ComboBox_AddString(m_cmbWindowScope, L"ทำงานทุกหน้าต่างทั่วทั้งระบบ (Global)");
    ComboBox_AddString(m_cmbWindowScope, L"ทำงานเฉพาะหน้าต่างเบราว์เซอร์และ Instagram");

    m_chkBeep = CreateWindowExW(0, L"BUTTON", L"เปิดเสียงแจ้งเตือน (Beep Sound)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 296, 320, 24, m_hWnd, (HMENU)IDC_CHK_BEEP, m_hInstance, NULL);
    SendMessageW(m_chkBeep, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    m_chkAutoStart = CreateWindowExW(0, L"BUTTON", L"เปิดโปรแกรมอัตโนมัติเมื่อเข้าหน้าเว็บ Instagram",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 326, 420, 24, m_hWnd, (HMENU)IDC_CHK_AUTO_START, m_hInstance, NULL);
    SendMessageW(m_chkAutoStart, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // 4. คำแนะนำ
    HWND lblHelp = CreateWindowExW(0, L"STATIC", L"คำแนะนำ: ตั้งปุ่มคีย์บอร์ด, เมาส์ หรือกดปุ่มร่วม (เช่น Ctrl+M) | ปิดฉุกเฉิน: Ctrl+Alt+Q",
        WS_CHILD | WS_VISIBLE,
        20, 380, 485, 20, m_hWnd, NULL, m_hInstance, NULL);
    SendMessageW(lblHelp, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // 5. แถบสถานะ
    m_lblStatus = CreateWindowExW(0, L"STATIC", L"สถานะ: เซิร์ฟเวอร์พร้อมใช้งาน (พอร์ต 18888) | เบราว์เซอร์เชื่อมต่อ: 0",
        WS_CHILD | WS_VISIBLE,
        20, 405, 485, 20, m_hWnd, (HMENU)IDC_LBL_STATUS, m_hInstance, NULL);
    SendMessageW(m_lblStatus, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // 6. ปุ่มตรวจสอบ/ติดตั้ง Extension
    m_btnInstallExt = CreateWindowExW(0, L"BUTTON", L"ตรวจสอบ / ติดตั้ง Chrome Extension",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 435, 485, 36, m_hWnd, (HMENU)IDC_BTN_INSTALL_EXT, m_hInstance, NULL);
    SendMessageW(m_btnInstallExt, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // 7. ปุ่มบันทึกและทำงานพื้นหลัง
    m_btnSaveHide = CreateWindowExW(0, L"BUTTON", L"บันทึกและทำงานในพื้นหลัง (ซ่อนหน้าต่าง)",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        20, 480, 485, 36, m_hWnd, (HMENU)IDC_BTN_SAVE_HIDE, m_hInstance, NULL);
    SendMessageW(m_btnSaveHide, WM_SETFONT, (WPARAM)m_hFont, TRUE);
}

void MainWindow::LoadConfigToUI() {
    UpdateKeyButtons();

    ComboBox_SetCurSel(m_cmbMicMode, m_config.micMode == 1 ? 1 : 0);
    ComboBox_SetCurSel(m_cmbAudioMode, m_config.audioMode == 1 ? 1 : 0);
    ComboBox_SetCurSel(m_cmbWindowScope, m_config.windowScope == 1 ? 1 : 0);

    Button_SetCheck(m_chkBeep, m_config.beepEnabled ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(m_chkAutoStart, m_config.autoStartWithWeb ? BST_CHECKED : BST_UNCHECKED);
}

void MainWindow::SaveConfigFromUI() {
    m_config.micMode = ComboBox_GetCurSel(m_cmbMicMode);
    m_config.audioMode = ComboBox_GetCurSel(m_cmbAudioMode);
    m_config.windowScope = ComboBox_GetCurSel(m_cmbWindowScope);
    m_config.beepEnabled = (Button_GetCheck(m_chkBeep) == BST_CHECKED);
    m_config.autoStartWithWeb = (Button_GetCheck(m_chkAutoStart) == BST_CHECKED);

    SaveConfig(GetIniFilePath(), m_config);
    HotkeyHook::UpdateConfig(m_config);

    // ลงทะเบียนหรือยกเลิก Native Messaging Host ตาม Checkbox
    SetAutoStartRegistered(m_config.autoStartWithWeb);

    // ส่งข้อความ sync_state ครบชุดไปยัง Browser
    std::string syncMsg = "{\"action\":\"sync_state\","
        "\"mic_active\":" + std::string(HotkeyHook::GetMicActive() ? "true" : "false") + ","
        "\"audio_active\":" + std::string(HotkeyHook::GetAudioActive() ? "true" : "false") + ","
        "\"mic_mode\":" + std::to_string(m_config.micMode) + ","
        "\"audio_mode\":" + std::to_string(m_config.audioMode) + ","
        "\"beep_enabled\":" + (m_config.beepEnabled ? "true" : "false") + ","
        "\"auto_start_enabled\":" + (m_config.autoStartWithWeb ? "true" : "false") + "}";
    g_wsServer.Broadcast(syncMsg);
}

void MainWindow::OnMicModeChanged() {
    m_config.micMode = ComboBox_GetCurSel(m_cmbMicMode);
    if (m_config.micMode == 1) { // โหมด Push-to-Talk ปิดไมค์ทันที
        HotkeyHook::SetMicActive(false);
    } else { // โหมด Toggle เปิดไมค์เป็นค่าเริ่มต้น
        HotkeyHook::SetMicActive(true);
    }
    SaveConfig(GetIniFilePath(), m_config);
    HotkeyHook::UpdateConfig(m_config);

    // ซิงค์สถานะใหม่แบบเรียลไทม์ทันทีที่เลือก
    std::string syncMsg = "{\"action\":\"sync_state\","
        "\"mic_active\":" + std::string(HotkeyHook::GetMicActive() ? "true" : "false") + ","
        "\"audio_active\":" + std::string(HotkeyHook::GetAudioActive() ? "true" : "false") + ","
        "\"mic_mode\":" + std::to_string(m_config.micMode) + ","
        "\"audio_mode\":" + std::to_string(m_config.audioMode) + ","
        "\"beep_enabled\":" + (m_config.beepEnabled ? "true" : "false") + ","
        "\"auto_start_enabled\":" + (m_config.autoStartWithWeb ? "true" : "false") + "}";
    g_wsServer.Broadcast(syncMsg);
}

void MainWindow::OnAudioModeChanged() {
    m_config.audioMode = ComboBox_GetCurSel(m_cmbAudioMode);
    HotkeyHook::SetAudioActive(true);
    SaveConfig(GetIniFilePath(), m_config);
    HotkeyHook::UpdateConfig(m_config);

    std::string syncMsg = "{\"action\":\"sync_state\","
        "\"mic_active\":" + std::string(HotkeyHook::GetMicActive() ? "true" : "false") + ","
        "\"audio_active\":" + std::string(HotkeyHook::GetAudioActive() ? "true" : "false") + ","
        "\"mic_mode\":" + std::to_string(m_config.micMode) + ","
        "\"audio_mode\":" + std::to_string(m_config.audioMode) + ","
        "\"beep_enabled\":" + (m_config.beepEnabled ? "true" : "false") + ","
        "\"auto_start_enabled\":" + (m_config.autoStartWithWeb ? "true" : "false") + "}";
    g_wsServer.Broadcast(syncMsg);
}

void MainWindow::OnScopeChanged() {
    m_config.windowScope = ComboBox_GetCurSel(m_cmbWindowScope);
    SaveConfig(GetIniFilePath(), m_config);
    HotkeyHook::UpdateConfig(m_config);
}

void MainWindow::OnBeepToggled() {
    m_config.beepEnabled = (Button_GetCheck(m_chkBeep) == BST_CHECKED);
    SaveConfig(GetIniFilePath(), m_config);
    HotkeyHook::UpdateConfig(m_config);
    g_wsServer.Broadcast("{\"action\":\"config_update\",\"beep_enabled\":" + std::string(m_config.beepEnabled ? "true" : "false") + "}");
}

void MainWindow::OnAutoStartToggled() {
    m_config.autoStartWithWeb = (Button_GetCheck(m_chkAutoStart) == BST_CHECKED);
    SaveConfig(GetIniFilePath(), m_config);
    SetAutoStartRegistered(m_config.autoStartWithWeb);
}

void MainWindow::UpdateKeyButtons() {
    if (!m_recordingMic) {
        std::wstring text = L"[ " + GetBindingDisplayName(m_config.micHotkey) + L" ] คลิกเพื่อเปลี่ยน";
        SetWindowTextW(m_btnMicKey, text.c_str());
    } else {
        SetWindowTextW(m_btnMicKey, L"กำลังรอ... กดปุ่มหรือคลิกเมาส์");
    }

    if (!m_recordingAudio) {
        std::wstring text = L"[ " + GetBindingDisplayName(m_config.audioHotkey) + L" ] คลิกเพื่อเปลี่ยน";
        SetWindowTextW(m_btnAudioKey, text.c_str());
    } else {
        SetWindowTextW(m_btnAudioKey, L"กำลังรอ... กดปุ่มหรือคลิกเมาส์");
    }
}

void MainWindow::UpdateClientCount(int count) {
    m_connectedClients = count;
    std::wstring text = L"สถานะ: เซิร์ฟเวอร์พร้อมใช้งาน (พอร์ต 18888) | เบราว์เซอร์เชื่อมต่อ: " + std::to_wstring(count);
    SetWindowTextW(m_lblStatus, text.c_str());
}

void MainWindow::OnKeyRecorded(int target, const HotkeyBinding& binding) {
    if (target == (int)RecordTarget::Mic) {
        m_config.micHotkey = binding;
        m_recordingMic = false;
    } else if (target == (int)RecordTarget::Audio) {
        m_config.audioHotkey = binding;
        m_recordingAudio = false;
    }
    UpdateKeyButtons();
    SaveConfigFromUI();
}

void MainWindow::RestoreWindow() {
    ShowWindow(m_hWnd, SW_SHOW);
    ShowWindow(m_hWnd, SW_RESTORE);
    SetForegroundWindow(m_hWnd);
}

void MainWindow::ExitApplication() {
    RemoveSystemTray();
    DestroyWindow(m_hWnd);
    PostQuitMessage(0);
}

void MainWindow::OnInstallExtClicked() {
    ExtensionInstaller::InstallAndOpenInBrowser(m_hWnd);
}

void MainWindow::InitSystemTray() {
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hWnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    wcscpy_s(m_nid.szTip, L"IG Audio Controller");

    Shell_NotifyIconW(NIM_ADD, &m_nid);
}

void MainWindow::RemoveSystemTray() {
    Shell_NotifyIconW(NIM_DELETE, &m_nid);
}

void MainWindow::ShowTrayMenu() {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_SHOW, L"แสดงหน้าต่างตั้งค่า");
    InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"ออกจากโปรแกรม");

    SetForegroundWindow(m_hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, m_hWnd, NULL);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = g_pApp;

    switch (msg) {
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == IDC_CMB_MIC_MODE && wmEvent == CBN_SELCHANGE) {
                if (pThis) pThis->OnMicModeChanged();
            } else if (wmId == IDC_CMB_AUDIO_MODE && wmEvent == CBN_SELCHANGE) {
                if (pThis) pThis->OnAudioModeChanged();
            } else if (wmId == IDC_CMB_WINDOW_SCOPE && wmEvent == CBN_SELCHANGE) {
                if (pThis) pThis->OnScopeChanged();
            } else if (wmId == IDC_CHK_BEEP && wmEvent == BN_CLICKED) {
                if (pThis) pThis->OnBeepToggled();
            } else if (wmId == IDC_CHK_AUTO_START && wmEvent == BN_CLICKED) {
                if (pThis) pThis->OnAutoStartToggled();
            } else if (wmId == IDC_BTN_INSTALL_EXT) {
                if (pThis) pThis->OnInstallExtClicked();
            } else if (wmId == IDC_BTN_MIC_KEY) {
                if (pThis) {
                    pThis->m_recordingAudio = false;
                    pThis->m_recordingMic = true;
                    pThis->UpdateKeyButtons();
                    HotkeyHook::StartRecording(RecordTarget::Mic);
                }
            } else if (wmId == IDC_BTN_AUDIO_KEY) {
                if (pThis) {
                    pThis->m_recordingMic = false;
                    pThis->m_recordingAudio = true;
                    pThis->UpdateKeyButtons();
                    HotkeyHook::StartRecording(RecordTarget::Audio);
                }
            } else if (wmId == IDC_BTN_SAVE_HIDE) {
                if (pThis) {
                    pThis->SaveConfigFromUI();
                    ShowWindow(hWnd, SW_HIDE);

                    pThis->m_nid.uFlags |= NIF_INFO;
                    wcscpy_s(pThis->m_nid.szInfoTitle, L"IG Audio Controller");
                    wcscpy_s(pThis->m_nid.szInfo, L"โปรแกรมกำลังทำงานในพื้นหลัง ดับเบิ้ลคลิกไอคอนเพื่อเปิดหน้าต่างอีกครั้ง");
                    Shell_NotifyIconW(NIM_MODIFY, &pThis->m_nid);
                }
            } else if (wmId == ID_TRAY_SHOW) {
                if (pThis) pThis->RestoreWindow();
            } else if (wmId == ID_TRAY_EXIT) {
                if (pThis) pThis->ExitApplication();
            }
            break;
        }

        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONDBLCLK) {
                if (pThis) pThis->RestoreWindow();
            } else if (lParam == WM_RBUTTONUP) {
                if (pThis) pThis->ShowTrayMenu();
            }
            break;
        }

        case WM_CLIENT_COUNT_CHANGED: {
            if (pThis) pThis->UpdateClientCount((int)wParam);
            break;
        }

        case WM_KEY_RECORDED: {
            if (pThis) {
                HotkeyBinding b;
                b.vkCode = (DWORD)(lParam & 0xFFFF);
                b.ctrl = (lParam & 0x10000) != 0;
                b.alt = (lParam & 0x20000) != 0;
                b.shift = (lParam & 0x40000) != 0;
                b.win = (lParam & 0x80000) != 0;
                pThis->OnKeyRecorded((int)wParam, b);
            }
            break;
        }

        case WM_CLOSE: {
            if (pThis) {
                pThis->SaveConfigFromUI();
                ShowWindow(hWnd, SW_HIDE);
            }
            return 0;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
