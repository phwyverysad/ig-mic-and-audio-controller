#include "native_host.h"
#include <fstream>
#include <vector>

static void RegisterKeyInRegistry(const wchar_t* subKey, const std::wstring& jsonPath) {
    HKEY hKey = NULL;
    LONG res = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        subKey,
        0, NULL, REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE, NULL, &hKey, NULL
    );
    if (res == ERROR_SUCCESS && hKey != NULL) {
        RegSetValueExW(
            hKey,
            NULL,
            0,
            REG_SZ,
            (const BYTE*)jsonPath.c_str(),
            (DWORD)((jsonPath.size() + 1) * sizeof(wchar_t))
        );
        RegCloseKey(hKey);
    }
}

static void UnregisterKeyFromRegistry(const wchar_t* subKey) {
    RegDeleteKeyW(HKEY_CURRENT_USER, subKey);
}

void RegisterNativeHost() {
    wchar_t exePathBuf[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePathBuf, MAX_PATH);
    std::wstring exePath(exePathBuf);

    size_t pos = exePath.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return;

    std::wstring dir = exePath.substr(0, pos + 1);
    std::wstring jsonPath = dir + L"com.instagram.hotkey.controller.json";

    std::wstring escapedExe = L"";
    for (wchar_t c : exePath) {
        if (c == L'\\') escapedExe += L"\\\\";
        else escapedExe += c;
    }

    std::string jsonContent = "{\n"
        "  \"name\": \"com.instagram.hotkey.controller\",\n"
        "  \"description\": \"Instagram Hotkey Controller Host\",\n"
        "  \"path\": \"";

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, escapedExe.c_str(), -1, NULL, 0, NULL, NULL);
    std::vector<char> utf8Buf(utf8Len, 0);
    WideCharToMultiByte(CP_UTF8, 0, escapedExe.c_str(), -1, utf8Buf.data(), utf8Len, NULL, NULL);
    jsonContent += utf8Buf.data();

    jsonContent += "\",\n"
        "  \"type\": \"stdio\",\n"
        "  \"allowed_origins\": [\n"
        "    \"chrome-extension://ljnimllanldnbhcibpeggokmgpmphdoh/\"\n"
        "  ]\n"
        "}\n";

    std::ofstream outFile(jsonPath, std::ios::binary);
    if (outFile.is_open()) {
        outFile.write(jsonContent.data(), jsonContent.size());
        outFile.close();
    }

    RegisterKeyInRegistry(L"Software\\Google\\Chrome\\NativeMessagingHosts\\com.instagram.hotkey.controller", jsonPath);
    RegisterKeyInRegistry(L"Software\\Microsoft\\Edge\\NativeMessagingHosts\\com.instagram.hotkey.controller", jsonPath);
    RegisterKeyInRegistry(L"Software\\BraveSoftware\\Brave-Browser\\NativeMessagingHosts\\com.instagram.hotkey.controller", jsonPath);
}

void UnregisterNativeHost() {
    UnregisterKeyFromRegistry(L"Software\\Google\\Chrome\\NativeMessagingHosts\\com.instagram.hotkey.controller");
    UnregisterKeyFromRegistry(L"Software\\Microsoft\\Edge\\NativeMessagingHosts\\com.instagram.hotkey.controller");
    UnregisterKeyFromRegistry(L"Software\\BraveSoftware\\Brave-Browser\\NativeMessagingHosts\\com.instagram.hotkey.controller");
}

void SetAutoStartRegistered(bool enable) {
    if (enable) {
        RegisterNativeHost();
    } else {
        UnregisterNativeHost();
    }
}

bool IsNativeMessagingLaunch() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == NULL || hIn == INVALID_HANDLE_VALUE) {
        return false;
    }
    return (GetFileType(hIn) == FILE_TYPE_PIPE);
}

bool HandleNativeMessagingIO(bool isDuplicateInstance) {
    if (!IsNativeMessagingLaunch()) {
        return false;
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    if (hOut == NULL || hOut == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD bytesAvail = 0;
    for (int i = 0; i < 100; ++i) { // รอข้อมูลจาก Chrome สูงสุด 1000ms
        if (PeekNamedPipe(hIn, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail >= 4) {
            break;
        }
        Sleep(10);
    }

    if (bytesAvail >= 4) {
        uint32_t msgLen = 0;
        DWORD bytesRead = 0;
        if (ReadFile(hIn, &msgLen, 4, &bytesRead, NULL) && bytesRead == 4) {
            if (msgLen > 0 && msgLen < 65536) {
                std::vector<char> buf(msgLen + 1, 0);
                ReadFile(hIn, buf.data(), msgLen, &bytesRead, NULL);
            }

            std::string reply = isDuplicateInstance ? "{\"status\":\"already_running\"}" : "{\"status\":\"ok\"}";
            uint32_t replyLen = (uint32_t)reply.size();
            DWORD written = 0;
            WriteFile(hOut, &replyLen, 4, &written, NULL);
            WriteFile(hOut, reply.data(), replyLen, &written, NULL);
            return true;
        }
    }
    return false;
}
