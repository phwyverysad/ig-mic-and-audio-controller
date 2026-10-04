#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include "../cpp_hotkey/ws_server.h"

int main() {
    WebSocketServer server;
    bool clientMsgReceived = false;

    if (!server.Start(
        18890,
        nullptr,
        [&server](SOCKET sock) {
            // ส่ง Initial State Sync ทันทีที่เชื่อมต่อ
            server.SendTo(sock, "{\"action\":\"sync_state\",\"mic_active\":false,\"audio_active\":true,\"mic_mode\":1,\"audio_mode\":0,\"beep_enabled\":true}");
        },
        [&clientMsgReceived](const std::string& msg, SOCKET sender) {
            if (msg.find("ui_state_change") != std::string::npos) {
                clientMsgReceived = true;
            }
        }
    )) {
        std::cerr << "Failed to start server on port 18890" << std::endl;
        return 1;
    }

    std::cout << "SERVER_READY" << std::endl;

    // รอ client เชื่อมต่อ
    for (int i = 0; i < 50; ++i) {
        if (server.GetClientCount() > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (server.GetClientCount() == 0) {
        std::cerr << "No client connected within timeout" << std::endl;
        server.Stop();
        return 2;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // ส่งชุดคำสั่ง Push-to-Talk และ Toggle
    server.Broadcast("{\"action\":\"set_mic\",\"state\":true}");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    server.Broadcast("{\"action\":\"set_mic\",\"state\":false}");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    server.Broadcast("{\"action\":\"set_audio\",\"state\":false}");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    server.Broadcast("{\"action\":\"set_audio\",\"state\":true}");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // รอรับข้อความตอบกลับจาก client
    for (int i = 0; i < 20; ++i) {
        if (clientMsgReceived) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    server.Stop();
    return clientMsgReceived ? 0 : 3;
}
