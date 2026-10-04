#include "config.h"

AppConfig GetDefaultConfig() {
    AppConfig cfg;
    cfg.micHotkey = { VK_F8, false, false, false, false };
    cfg.micMode = 0;       // Toggle
    cfg.audioHotkey = { VK_F9, false, false, false, false };
    cfg.audioMode = 0;     // Toggle
    cfg.windowScope = 0;   // Global
    cfg.beepEnabled = true;
    cfg.autoStartWithWeb = true; // เปิดโปรแกรมอัตโนมัติเมื่อเข้าหน้าเว็บ Instagram เป็นค่าเริ่มต้น
    return cfg;
}

std::wstring GetIniFilePath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring str(path);
    size_t pos = str.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return str.substr(0, pos + 1) + L"hotkeys.ini";
    }
    return L"hotkeys.ini";
}

AppConfig LoadConfig(const std::wstring& filePath) {
    AppConfig cfg = GetDefaultConfig();
    const wchar_t* path = filePath.c_str();

    // โหลดปุ่มไมค์
    cfg.micHotkey.vkCode = (DWORD)GetPrivateProfileIntW(L"Hotkeys", L"MicKey", (int)cfg.micHotkey.vkCode, path);
    cfg.micHotkey.ctrl = GetPrivateProfileIntW(L"Hotkeys", L"MicCtrl", 0, path) != 0;
    cfg.micHotkey.alt = GetPrivateProfileIntW(L"Hotkeys", L"MicAlt", 0, path) != 0;
    cfg.micHotkey.shift = GetPrivateProfileIntW(L"Hotkeys", L"MicShift", 0, path) != 0;
    cfg.micHotkey.win = GetPrivateProfileIntW(L"Hotkeys", L"MicWin", 0, path) != 0;
    cfg.micMode = GetPrivateProfileIntW(L"Hotkeys", L"MicMode", cfg.micMode, path);

    // โหลดปุ่มหูฟัง
    cfg.audioHotkey.vkCode = (DWORD)GetPrivateProfileIntW(L"Hotkeys", L"AudioKey", (int)cfg.audioHotkey.vkCode, path);
    cfg.audioHotkey.ctrl = GetPrivateProfileIntW(L"Hotkeys", L"AudioCtrl", 0, path) != 0;
    cfg.audioHotkey.alt = GetPrivateProfileIntW(L"Hotkeys", L"AudioAlt", 0, path) != 0;
    cfg.audioHotkey.shift = GetPrivateProfileIntW(L"Hotkeys", L"AudioShift", 0, path) != 0;
    cfg.audioHotkey.win = GetPrivateProfileIntW(L"Hotkeys", L"AudioWin", 0, path) != 0;
    cfg.audioMode = GetPrivateProfileIntW(L"Hotkeys", L"AudioMode", cfg.audioMode, path);

    // การตั้งค่าทั่วไป
    cfg.windowScope = GetPrivateProfileIntW(L"General", L"WindowScope", cfg.windowScope, path);
    cfg.beepEnabled = GetPrivateProfileIntW(L"General", L"BeepEnabled", cfg.beepEnabled ? 1 : 0, path) != 0;
    cfg.autoStartWithWeb = GetPrivateProfileIntW(L"General", L"AutoStartWithWeb", cfg.autoStartWithWeb ? 1 : 0, path) != 0;

    return cfg;
}

void SaveConfig(const std::wstring& filePath, const AppConfig& config) {
    const wchar_t* path = filePath.c_str();

    // บันทึกปุ่มไมค์
    WritePrivateProfileStringW(L"Hotkeys", L"MicKey", std::to_wstring(config.micHotkey.vkCode).c_str(), path);
    WritePrivateProfileStringW(L"Hotkeys", L"MicCtrl", config.micHotkey.ctrl ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"Hotkeys", L"MicAlt", config.micHotkey.alt ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"Hotkeys", L"MicShift", config.micHotkey.shift ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"Hotkeys", L"MicWin", config.micHotkey.win ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"Hotkeys", L"MicMode", std::to_wstring(config.micMode).c_str(), path);

    // บันทึกปุ่มหูฟัง
    WritePrivateProfileStringW(L"Hotkeys", L"AudioKey", std::to_wstring(config.audioHotkey.vkCode).c_str(), path);
    WritePrivateProfileStringW(L"Hotkeys", L"AudioCtrl", config.audioHotkey.ctrl ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"Hotkeys", L"AudioAlt", config.audioHotkey.alt ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"Hotkeys", L"AudioShift", config.audioHotkey.shift ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"Hotkeys", L"AudioWin", config.audioHotkey.win ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"Hotkeys", L"AudioMode", std::to_wstring(config.audioMode).c_str(), path);

    // บันทึกการตั้งค่าทั่วไป
    WritePrivateProfileStringW(L"General", L"WindowScope", std::to_wstring(config.windowScope).c_str(), path);
    WritePrivateProfileStringW(L"General", L"BeepEnabled", config.beepEnabled ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"General", L"AutoStartWithWeb", config.autoStartWithWeb ? L"1" : L"0", path);
}
