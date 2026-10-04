#pragma once
#include <windows.h>
#include <string>

namespace ExtensionInstaller {
    // หาตำแหน่งโฟลเดอร์ของ Extension (โฟลเดอร์ extension ภายในโปรเจกต์ หรือใน LocalAppData)
    std::wstring GetExtensionPath();

    // ตรวจสอบว่ามีไฟล์ Extension (manifest.json) อยู่หรือไม่
    bool IsExtensionFilesPresent();

    // ดาวน์โหลดไฟล์ Extension จาก GitHub Repository อัตโนมัติหากยังไม่มี
    bool DownloadExtensionFromGitHub(HWND hWndParent = NULL);

    // ติดตั้งและเปิดใช้งาน Extension บนเบราว์เซอร์อัตโนมัติ
    bool InstallAndOpenInBrowser(HWND hWndParent = NULL);
}
