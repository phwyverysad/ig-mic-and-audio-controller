#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

struct HotkeyBinding {
    DWORD vkCode;     // Virtual-Key Code หรือ Mouse Button (เช่น VK_F8, VK_XBUTTON1, 'M')
    bool ctrl;        // ต้องกด Ctrl ร่วมด้วยหรือไม่
    bool alt;         // ต้องกด Alt ร่วมด้วยหรือไม่
    bool shift;       // ต้องกด Shift ร่วมด้วยหรือไม่
    bool win;         // ต้องกด Win ร่วมด้วยหรือไม่
};

struct AppConfig {
    HotkeyBinding micHotkey;      // ปุ่มไมโครโฟน
    int micMode;                  // 0 = Toggle (สลับเปิด/ปิด), 1 = Push-to-Talk (กดค้าง)
    HotkeyBinding audioHotkey;    // ปุ่มหูฟัง/เสียง
    int audioMode;                // 0 = Toggle (สลับเปิด/ปิด), 1 = Push-to-Mute (กดค้างปิดเสียง)
    int windowScope;              // 0 = ทำงานทุกหน้าต่าง (Global), 1 = ทำงานเฉพาะหน้าต่างเบราว์เซอร์/Instagram
    bool beepEnabled;             // true = เปิดเสียงแจ้งเตือน Beep, false = ปิดเสียงแจ้งเตือน
    bool autoStartWithWeb;        // true = เปิดโปรแกรมอัตโนมัติเมื่อเข้าหน้าเว็บ Instagram
};

AppConfig GetDefaultConfig();
std::wstring GetIniFilePath();
AppConfig LoadConfig(const std::wstring& filePath);
void SaveConfig(const std::wstring& filePath, const AppConfig& config);
