#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

#include <fstream>
#include <vector>

#include "../cpp_hotkey/config.h"
#include "../cpp_hotkey/key_names.h"
#include "../cpp_hotkey/ws_server.h"
#include "../cpp_hotkey/native_host.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

void TestConfigManagementWithCombosAndMouse() {
    std::cout << "[RUNNING] Test 1: Config Save & Load with Combos & Mouse Buttons... " << std::endl;

    std::wstring testIni = L".\\test_hotkeys.ini";
    DeleteFileW(testIni.c_str());

    AppConfig original;
    // ตั้งค่าปุ่มไมค์เป็น Ctrl + Shift + Mouse 4
    original.micHotkey = { VK_XBUTTON1, true, false, true, false };
    original.micMode = 1;       // Push-to-Talk

    // ตั้งค่าปุ่มหูฟังเป็น Alt + Media Play/Pause
    original.audioHotkey = { VK_MEDIA_PLAY_PAUSE, false, true, false, false };
    original.audioMode = 1;     // Push-to-Mute

    original.windowScope = 1;   // Browser only
    original.beepEnabled = false;
    original.autoStartWithWeb = true;

    SaveConfig(testIni, original);

    AppConfig loaded = LoadConfig(testIni);
    assert(loaded.micHotkey.vkCode == VK_XBUTTON1);
    assert(loaded.micHotkey.ctrl == true);
    assert(loaded.micHotkey.alt == false);
    assert(loaded.micHotkey.shift == true);
    assert(loaded.micHotkey.win == false);
    assert(loaded.micMode == 1);

    assert(loaded.audioHotkey.vkCode == VK_MEDIA_PLAY_PAUSE);
    assert(loaded.audioHotkey.ctrl == false);
    assert(loaded.audioHotkey.alt == true);
    assert(loaded.audioHotkey.shift == false);
    assert(loaded.audioHotkey.win == false);
    assert(loaded.audioMode == 1);

    assert(loaded.windowScope == 1);
    assert(loaded.beepEnabled == false);
    assert(loaded.autoStartWithWeb == true);

    DeleteFileW(testIni.c_str());
    std::cout << "  [PASS] Combos (Ctrl+Shift+Mouse4) and modes correctly saved and loaded." << std::endl;
}

void TestKeyDisplayNamesWithMouseAndCombos() {
    std::cout << "[RUNNING] Test 2: Key Names & Combo Display Mapping... " << std::endl;

    // Mouse Buttons
    assert(GetKeyDisplayName(VK_LBUTTON) == L"Mouse Left");
    assert(GetKeyDisplayName(VK_RBUTTON) == L"Mouse Right");
    assert(GetKeyDisplayName(VK_MBUTTON) == L"Mouse Middle");
    assert(GetKeyDisplayName(VK_XBUTTON1) == L"Mouse 4");
    assert(GetKeyDisplayName(VK_XBUTTON2) == L"Mouse 5");

    // Combos Display
    HotkeyBinding combo1 = { VK_XBUTTON1, true, false, false, false };
    assert(GetBindingDisplayName(combo1) == L"Ctrl + Mouse 4");

    HotkeyBinding combo2 = { VK_F8, true, true, false, false };
    assert(GetBindingDisplayName(combo2) == L"Ctrl + Alt + F8");

    HotkeyBinding combo3 = { 'M', true, false, true, false };
    assert(GetBindingDisplayName(combo3) == L"Ctrl + Shift + M");

    HotkeyBinding singleMouse = { VK_XBUTTON2, false, false, false, false };
    assert(GetBindingDisplayName(singleMouse) == L"Mouse 5");

    HotkeyBinding singleKey = { VK_F9, false, false, false, false };
    assert(GetBindingDisplayName(singleKey) == L"F9");

    std::cout << "  [PASS] Mouse buttons and combination keys mapped properly." << std::endl;
}

void TestWebSocketBidirectionalSync() {
    std::cout << "[RUNNING] Test 3: WebSocket RFC-6455 Bidirectional Sync... " << std::endl;

    WebSocketServer server;
    int clientCountReported = 0;
    bool clientConnectedReported = false;
    std::string receivedClientMsg = "";

    int testPort = 18889;
    bool started = server.Start(
        testPort,
        [&](int count) {
            clientCountReported = count;
        },
        [&](SOCKET sock) {
            clientConnectedReported = true;
            // ส่ง Initial State Sync
            server.SendTo(sock, "{\"action\":\"sync_state\",\"mic_active\":false,\"audio_active\":true,\"mic_mode\":1,\"audio_mode\":0,\"beep_enabled\":true}");
        },
        [&](const std::string& msg, SOCKET sender) {
            receivedClientMsg = msg;
        }
    );
    assert(started == true);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Connect test TCP client
    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(clientSock != INVALID_SOCKET);

    sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons((u_short)testPort);

    int connectRes = connect(clientSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    assert(connectRes == 0);

    // Send HTTP upgrade request
    std::string handshakeReq =
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1:18889\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";

    send(clientSock, handshakeReq.data(), (int)handshakeReq.size(), 0);

    char respBuf[1024] = { 0 };
    int r = recv(clientSock, respBuf, sizeof(respBuf) - 1, 0);
    assert(r > 0);
    std::string respStr(respBuf, r);
    assert(respStr.find("101 Switching Protocols") != std::string::npos);

    // รอรับ Initial State frame ที่เซิร์ฟเวอร์ส่งมาให้ทันทีหลังเชื่อมต่อ
    unsigned char frameHeader[2];
    int hr = recv(clientSock, (char*)frameHeader, 2, 0);
    assert(hr == 2);
    int payloadLen = frameHeader[1] & 0x7F;
    std::vector<char> syncPayload(payloadLen + 1, 0);
    int pr = recv(clientSock, syncPayload.data(), payloadLen, 0);
    assert(pr == payloadLen);
    assert(std::string(syncPayload.data()).find("sync_state") != std::string::npos);
    std::cout << "  [PASS] Initial state sync immediately received by client." << std::endl;

    // ไคลเอนต์ส่งข้อความ Masked Text Frame ไปยังเซิร์ฟเวอร์ (เลียนแบบ Browser ส่ง ui_state_change)
    std::string clientMsg = "{\"action\":\"ui_state_change\",\"type\":\"mic\",\"state\":true}";
    std::vector<unsigned char> clientFrame;
    clientFrame.push_back(0x81); // FIN + Text
    clientFrame.push_back(0x80 | (unsigned char)clientMsg.size()); // Masked bit set
    unsigned char maskKey[4] = { 0x12, 0x34, 0x56, 0x78 };
    clientFrame.insert(clientFrame.end(), maskKey, maskKey + 4);
    for (size_t i = 0; i < clientMsg.size(); ++i) {
        clientFrame.push_back((unsigned char)clientMsg[i] ^ maskKey[i % 4]);
    }

    send(clientSock, (const char*)clientFrame.data(), (int)clientFrame.size(), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    assert(receivedClientMsg == clientMsg);
    std::cout << "  [PASS] Server successfully unmasked and received client ui_state_change." << std::endl;

    // ปิดการเชื่อมต่อ
    closesocket(clientSock);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    server.Stop();
    std::cout << "  [PASS] Server cleanly shut down." << std::endl;
}

void TestNativeMessagingHostRegistrationAndProtocol() {
    std::cout << "[RUNNING] Test 4: Native Messaging Host Registry & Manifest... " << std::endl;

    // Test 4.1: Register Native Host
    SetAutoStartRegistered(true);

    HKEY hKey = NULL;
    LONG res = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Google\\Chrome\\NativeMessagingHosts\\com.instagram.hotkey.controller",
        0, KEY_READ, &hKey
    );
    assert(res == ERROR_SUCCESS);

    wchar_t jsonPathBuf[MAX_PATH] = { 0 };
    DWORD bufSize = sizeof(jsonPathBuf);
    res = RegQueryValueExW(hKey, NULL, NULL, NULL, (LPBYTE)jsonPathBuf, &bufSize);
    assert(res == ERROR_SUCCESS);
    RegCloseKey(hKey);

    std::wstring jsonPath(jsonPathBuf);
    assert(jsonPath.find(L"com.instagram.hotkey.controller.json") != std::wstring::npos);

    // Verify JSON file exists on disk
    DWORD attr = GetFileAttributesW(jsonPath.c_str());
    assert(attr != INVALID_FILE_ATTRIBUTES);

    // Verify JSON content
    std::ifstream jfile(jsonPath);
    assert(jfile.is_open());
    std::string content((std::istreambuf_iterator<char>(jfile)), std::istreambuf_iterator<char>());
    jfile.close();

    assert(content.find("com.instagram.hotkey.controller") != std::string::npos);
    assert(content.find("chrome-extension://ljnimllanldnbhcibpeggokmgpmphdoh/") != std::string::npos);
    assert(content.find("stdio") != std::string::npos);

    std::cout << "  [PASS] Native host registered in Chrome registry and valid JSON generated." << std::endl;

    // Test 4.2: Unregister Native Host
    SetAutoStartRegistered(false);
    hKey = NULL;
    res = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Google\\Chrome\\NativeMessagingHosts\\com.instagram.hotkey.controller",
        0, KEY_READ, &hKey
    );
    assert(res != ERROR_SUCCESS);
    std::cout << "  [PASS] Native host successfully unregistered and removed from registry." << std::endl;

    // Re-register production path
    DeleteFileW(jsonPath.c_str());
    std::wstring prodJson = L"..\\cpp_hotkey\\com.instagram.hotkey.controller.json";
    wchar_t fullProdJson[MAX_PATH] = { 0 };
    if (GetFullPathNameW(prodJson.c_str(), MAX_PATH, fullProdJson, NULL)) {
        HKEY hRegKey = NULL;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Google\\Chrome\\NativeMessagingHosts\\com.instagram.hotkey.controller",
            0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hRegKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hRegKey, NULL, 0, REG_SZ, (const BYTE*)fullProdJson, (DWORD)((wcslen(fullProdJson) + 1) * sizeof(wchar_t)));
            RegCloseKey(hRegKey);
        }
    }
    std::cout << "  [PASS] Native host re-registered to production path." << std::endl;
}

void TestNativeMessagingPipeIO() {
    std::cout << "[RUNNING] Test 5: Native Messaging Stdio Length-Prefixed Protocol... " << std::endl;

    HANDLE hInRead = NULL, hInWrite = NULL;
    HANDLE hOutRead = NULL, hOutWrite = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    assert(CreatePipe(&hInRead, &hInWrite, &sa, 0));
    assert(CreatePipe(&hOutRead, &hOutWrite, &sa, 0));

    HANDLE origIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE origOut = GetStdHandle(STD_OUTPUT_HANDLE);

    SetStdHandle(STD_INPUT_HANDLE, hInRead);
    SetStdHandle(STD_OUTPUT_HANDLE, hOutWrite);

    // Write a test message from "Chrome"
    std::string chromeMsg = "{\"action\":\"launch\"}";
    uint32_t msgLen = (uint32_t)chromeMsg.size();
    DWORD written = 0;
    WriteFile(hInWrite, &msgLen, 4, &written, NULL);
    WriteFile(hInWrite, chromeMsg.data(), msgLen, &written, NULL);

    bool handled = HandleNativeMessagingIO(false);
    assert(handled == true);

    uint32_t replyLen = 0;
    DWORD bytesRead = 0;
    assert(ReadFile(hOutRead, &replyLen, 4, &bytesRead, NULL) && bytesRead == 4);
    std::vector<char> replyBuf(replyLen + 1, 0);
    assert(ReadFile(hOutRead, replyBuf.data(), replyLen, &bytesRead, NULL) && bytesRead == replyLen);
    assert(std::string(replyBuf.data()) == "{\"status\":\"ok\"}");

    // Test duplicate instance
    WriteFile(hInWrite, &msgLen, 4, &written, NULL);
    WriteFile(hInWrite, chromeMsg.data(), msgLen, &written, NULL);

    handled = HandleNativeMessagingIO(true);
    assert(handled == true);

    assert(ReadFile(hOutRead, &replyLen, 4, &bytesRead, NULL) && bytesRead == 4);
    replyBuf.assign(replyLen + 1, 0);
    assert(ReadFile(hOutRead, replyBuf.data(), replyLen, &bytesRead, NULL) && bytesRead == replyLen);
    assert(std::string(replyBuf.data()) == "{\"status\":\"already_running\"}");

    // Restore original std handles
    SetStdHandle(STD_INPUT_HANDLE, origIn);
    SetStdHandle(STD_OUTPUT_HANDLE, origOut);

    CloseHandle(hInRead);
    CloseHandle(hInWrite);
    CloseHandle(hOutRead);
    CloseHandle(hOutWrite);

    std::cout << "  [PASS] Stdio length-prefixed Native Messaging handshake verified for both new and duplicate instances." << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Running Enhanced C++ Core Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    TestConfigManagementWithCombosAndMouse();
    TestKeyDisplayNamesWithMouseAndCombos();
    TestWebSocketBidirectionalSync();
    TestNativeMessagingHostRegistrationAndProtocol();
    TestNativeMessagingPipeIO();

    std::cout << "========================================" << std::endl;
    std::cout << "[ALL ENHANCED C++ TESTS PASSED!]" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
