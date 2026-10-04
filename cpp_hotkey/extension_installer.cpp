#include "extension_installer.h"
#include "native_host.h"
#include <shlobj.h>
#include <shellapi.h>
#include <vector>
#include <string>

namespace ExtensionInstaller {

static bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

static bool DirectoryExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

std::wstring GetExtensionPath() {
    wchar_t exePathBuf[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePathBuf, MAX_PATH);
    std::wstring exePath(exePathBuf);

    size_t pos = exePath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        std::wstring dir = exePath.substr(0, pos + 1);

        // ตรวจสอบตำแหน่งโฟลเดอร์ข้างเคียง (เช่น ../extension หรือ ./extension)
        std::wstring check1 = dir + L"..\\extension";
        wchar_t fullCheck1[MAX_PATH] = { 0 };
        if (GetFullPathNameW(check1.c_str(), MAX_PATH, fullCheck1, NULL)) {
            if (FileExists(std::wstring(fullCheck1) + L"\\manifest.json")) {
                return fullCheck1;
            }
        }

        std::wstring check2 = dir + L"extension";
        wchar_t fullCheck2[MAX_PATH] = { 0 };
        if (GetFullPathNameW(check2.c_str(), MAX_PATH, fullCheck2, NULL)) {
            if (FileExists(std::wstring(fullCheck2) + L"\\manifest.json")) {
                return fullCheck2;
            }
        }
    }

    // ค่าเริ่มต้น: โฟลเดอร์ใน LocalAppData
    wchar_t localAppBuf[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppBuf))) {
        return std::wstring(localAppBuf) + L"\\IGAudioController\\extension";
    }

    return L"";
}

bool IsExtensionFilesPresent() {
    std::wstring extPath = GetExtensionPath();
    if (extPath.empty()) return false;
    return FileExists(extPath + L"\\manifest.json");
}

bool DownloadExtensionFromGitHub(HWND hWndParent) {
    if (IsExtensionFilesPresent()) {
        return true;
    }

    std::wstring targetDir = GetExtensionPath();
    if (targetDir.empty()) {
        wchar_t localAppBuf[MAX_PATH] = { 0 };
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppBuf))) {
            targetDir = std::wstring(localAppBuf) + L"\\IGAudioController\\extension";
        }
    }

    // สร้างโฟลเดอร์ปลายทาง
    SHCreateDirectoryExW(NULL, targetDir.c_str(), NULL);

    // คำสั่ง PowerShell สำหรับดาวน์โหลด Extension จาก GitHub
    std::wstring psCmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \""
        L"$target = '" + targetDir + L"'; "
        L"if (!(Test-Path $target)) { New-Item -ItemType Directory -Force -Path $target | Out-Null }; "
        L"$tempRepo = Join-Path $env:TEMP ('ig_ext_' + [System.Guid]::NewGuid().ToString('N')); "
        L"$gitCmd = Get-Command git -ErrorAction SilentlyContinue; "
        L"if ($gitCmd) { "
        L"  git clone --depth 1 https://github.com/phwyverysad/ig-mic-and-audio-controller.git $tempRepo | Out-Null; "
        L"  if (Test-Path (Join-Path $tempRepo 'extension')) { "
        L"    Copy-Item -Recurse -Force (Join-Path $tempRepo 'extension\\*') $target; "
        L"  } "
        L"  Remove-Item -Recurse -Force $tempRepo -ErrorAction SilentlyContinue; "
        L"} else { "
        L"  $zipFile = Join-Path $env:TEMP 'ig_ext_download.zip'; "
        L"  [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
        L"  Invoke-WebRequest -Uri 'https://github.com/phwyverysad/ig-mic-and-audio-controller/archive/refs/heads/main.zip' -OutFile $zipFile; "
        L"  $extractPath = Join-Path $env:TEMP ('ig_ext_unzip_' + [System.Guid]::NewGuid().ToString('N')); "
        L"  Expand-Archive -Path $zipFile -DestinationPath $extractPath -Force; "
        L"  $foundExt = Get-ChildItem -Path $extractPath -Recurse -Directory -Filter 'extension' | Select-Object -First 1; "
        L"  if ($foundExt) { "
        L"    Copy-Item -Recurse -Force (Join-Path $foundExt.FullName '*') $target; "
        L"  } "
        L"  Remove-Item -Force $zipFile -ErrorAction SilentlyContinue; "
        L"  Remove-Item -Recurse -Force $extractPath -ErrorAction SilentlyContinue; "
        L"} "
        L"\"";

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::vector<wchar_t> cmdBuf(psCmd.begin(), psCmd.end());
    cmdBuf.push_back(0);

    if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 45000); // รอสูงสุด 45 วินาที
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return IsExtensionFilesPresent();
}

static std::wstring FindChromeExecutable() {
    wchar_t progFiles[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, 0, progFiles))) {
        std::wstring p1 = std::wstring(progFiles) + L"\\Google\\Chrome\\Application\\chrome.exe";
        if (FileExists(p1)) return p1;
    }

    wchar_t progFilesX86[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILESX86, NULL, 0, progFilesX86))) {
        std::wstring p2 = std::wstring(progFilesX86) + L"\\Google\\Chrome\\Application\\chrome.exe";
        if (FileExists(p2)) return p2;
    }

    wchar_t localApp[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localApp))) {
        std::wstring p3 = std::wstring(localApp) + L"\\Google\\Chrome\\Application\\chrome.exe";
        if (FileExists(p3)) return p3;
    }

    return L"";
}

bool InstallAndOpenInBrowser(HWND hWndParent) {
    // 1. ตรวจสอบและลงทะเบียน Native Messaging Host
    SetAutoStartRegistered(true);

    // 2. หากยังไม่มีไฟล์ Extension ให้ดาวน์โหลดอัตโนมัติ
    if (!IsExtensionFilesPresent()) {
        DownloadExtensionFromGitHub(hWndParent);
    }

    std::wstring extPath = GetExtensionPath();
    if (extPath.empty() || !IsExtensionFilesPresent()) {
        if (hWndParent) {
            MessageBoxW(hWndParent,
                L"ไม่สามารถค้นหาหรือดาวน์โหลด Extension ได้ กรุณาตรวจสอบการเชื่อมต่ออินเทอร์เน็ต",
                L"IG Audio Controller", MB_OK | MB_ICONWARNING);
        }
        return false;
    }

    // 3. ค้นหาเบราว์เซอร์ Chrome เพื่อเปิดและติดตั้ง Extension
    std::wstring chromePath = FindChromeExecutable();
    if (!chromePath.empty()) {
        std::wstring cmd = L"\"" + chromePath + L"\" --load-extension=\"" + extPath + L"\" https://www.instagram.com";
        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back(0);

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOW;

        if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    } else {
        // หากไม่พบ Chrome ให้เปิดโฟลเดอร์ Extension ใน File Explorer
        ShellExecuteW(hWndParent, L"open", extPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

    return true;
}

} // namespace ExtensionInstaller
