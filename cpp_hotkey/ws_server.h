#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <functional>

class WebSocketServer {
public:
    using ClientCountCallback = std::function<void(int)>;
    using ClientConnectedCallback = std::function<void(SOCKET)>;
    using MessageCallback = std::function<void(const std::string&, SOCKET)>;

    WebSocketServer();
    ~WebSocketServer();

    bool Start(
        int port,
        ClientCountCallback onClientCountChanged = nullptr,
        ClientConnectedCallback onClientConnected = nullptr,
        MessageCallback onMessage = nullptr
    );
    void Stop();
    void Broadcast(const std::string& textMessage, SOCKET excludeSender = INVALID_SOCKET);
    void SendTo(SOCKET clientSocket, const std::string& textMessage);
    int GetClientCount();

private:
    void ServerThread();
    void HandleClient(SOCKET clientSocket);
    bool PerformHandshake(SOCKET clientSocket, const std::string& request);
    void RemoveClient(SOCKET clientSocket);
    std::vector<unsigned char> EncodeTextFrame(const std::string& textMessage);

    int m_port;
    SOCKET m_listenSocket;
    bool m_running;
    std::thread m_serverThread;

    std::mutex m_clientsMutex;
    std::vector<SOCKET> m_clients;

    ClientCountCallback m_onClientCountChanged;
    ClientConnectedCallback m_onClientConnected;
    MessageCallback m_onMessage;
};
