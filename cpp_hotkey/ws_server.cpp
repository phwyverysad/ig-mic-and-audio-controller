#include "ws_server.h"
#include <wincrypt.h>
#include <iostream>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")

static std::string ComputeWebSocketAcceptKey(const std::string& clientKey) {
    std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = clientKey + magic;

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[20] = { 0 };
    DWORD hashLen = sizeof(hash);
    std::string result = "";

    if (CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
            if (CryptHashData(hHash, (const BYTE*)combined.data(), (DWORD)combined.size(), 0)) {
                if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
                    DWORD base64Len = 0;
                    if (CryptBinaryToStringA(hash, hashLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &base64Len)) {
                        std::vector<char> base64Buf(base64Len + 1, 0);
                        if (CryptBinaryToStringA(hash, hashLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, base64Buf.data(), &base64Len)) {
                            result = base64Buf.data();
                        }
                    }
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    return result;
}

WebSocketServer::WebSocketServer()
    : m_port(18888), m_listenSocket(INVALID_SOCKET), m_running(false) {
}

WebSocketServer::~WebSocketServer() {
    Stop();
}

bool WebSocketServer::Start(
    int port,
    ClientCountCallback onClientCountChanged,
    ClientConnectedCallback onClientConnected,
    MessageCallback onMessage
) {
    Stop();
    m_port = port;
    m_onClientCountChanged = onClientCountChanged;
    m_onClientConnected = onClientConnected;
    m_onMessage = onMessage;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    // สร้าง Dual-Stack Socket (รองรับทั้ง IPv4 และ IPv6 localhost)
    m_listenSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    bool useV6 = (m_listenSocket != INVALID_SOCKET);

    if (useV6) {
        DWORD v6only = 0;
        setsockopt(m_listenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&v6only, sizeof(v6only));

        int opt = 1;
        setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in6 serverAddr6 = { 0 };
        serverAddr6.sin6_family = AF_INET6;
        serverAddr6.sin6_addr = in6addr_any;
        serverAddr6.sin6_port = htons((u_short)m_port);

        if (bind(m_listenSocket, (sockaddr*)&serverAddr6, sizeof(serverAddr6)) == SOCKET_ERROR) {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
            useV6 = false;
        }
    }

    // Fallback เป็น IPv4 หาก IPv6 ไม่สำเร็จ
    if (!useV6) {
        m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSocket == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }

        int opt = 1;
        setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in serverAddr4 = { 0 };
        serverAddr4.sin_family = AF_INET;
        serverAddr4.sin_addr.s_addr = INADDR_ANY;
        serverAddr4.sin_port = htons((u_short)m_port);

        if (bind(m_listenSocket, (sockaddr*)&serverAddr4, sizeof(serverAddr4)) == SOCKET_ERROR) {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
            WSACleanup();
            return false;
        }
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    m_running = true;
    m_serverThread = std::thread(&WebSocketServer::ServerThread, this);
    return true;
}

void WebSocketServer::Stop() {
    if (!m_running) return;
    m_running = false;

    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (SOCKET s : m_clients) {
            closesocket(s);
        }
        m_clients.clear();
    }

    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }

    WSACleanup();

    if (m_onClientCountChanged) {
        m_onClientCountChanged(0);
    }
}

void WebSocketServer::ServerThread() {
    while (m_running) {
        sockaddr_storage clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(m_listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            if (!m_running) break;
            continue;
        }

        std::thread([this, clientSocket]() {
            HandleClient(clientSocket);
        }).detach();
    }
}

void WebSocketServer::HandleClient(SOCKET clientSocket) {
    char buffer[4096];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) {
        closesocket(clientSocket);
        return;
    }

    buffer[bytesReceived] = '\0';
    std::string request(buffer, bytesReceived);

    if (!PerformHandshake(clientSocket, request)) {
        closesocket(clientSocket);
        return;
    }

    int currentCount = 0;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        m_clients.push_back(clientSocket);
        currentCount = (int)m_clients.size();
    }
    if (m_onClientCountChanged) {
        m_onClientCountChanged(currentCount);
    }

    // แจ้งเตือนเมื่อ Client เชื่อมต่อสำเร็จ เพื่อส่ง Initial Sync State
    if (m_onClientConnected) {
        m_onClientConnected(clientSocket);
    }

    // รับ Frames จากไคลเอนต์
    while (m_running) {
        unsigned char header[2];
        int n = recv(clientSocket, (char*)header, 2, 0);
        if (n <= 0) break;

        unsigned char opcode = header[0] & 0x0F;
        bool masked = (header[1] & 0x80) != 0;
        uint64_t payloadLen = header[1] & 0x7F;

        if (payloadLen == 126) {
            unsigned char lenBytes[2];
            if (recv(clientSocket, (char*)lenBytes, 2, 0) <= 0) break;
            payloadLen = (lenBytes[0] << 8) | lenBytes[1];
        } else if (payloadLen == 127) {
            unsigned char lenBytes[8];
            if (recv(clientSocket, (char*)lenBytes, 8, 0) <= 0) break;
            payloadLen = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLen = (payloadLen << 8) | lenBytes[i];
            }
        }

        unsigned char maskKey[4] = { 0 };
        if (masked) {
            if (recv(clientSocket, (char*)maskKey, 4, 0) <= 0) break;
        }

        std::vector<char> payload((size_t)payloadLen);
        if (payloadLen > 0) {
            size_t totalRead = 0;
            while (totalRead < payloadLen) {
                int r = recv(clientSocket, payload.data() + totalRead, (int)(payloadLen - totalRead), 0);
                if (r <= 0) break;
                totalRead += r;
            }
            if (totalRead < payloadLen) break;
        }

        // ถอดรหัส Unmask ข้อมูลจากไคลเอนต์ตามมาตรฐาน RFC 6455
        if (masked && payloadLen > 0) {
            for (size_t i = 0; i < payloadLen; ++i) {
                payload[i] ^= maskKey[i % 4];
            }
        }

        // หากเป็น Close frame (0x8)
        if (opcode == 0x08) {
            unsigned char closeFrame[2] = { 0x88, 0x00 };
            send(clientSocket, (const char*)closeFrame, 2, 0);
            break;
        }

        // หากเป็น Ping frame (0x9) ตอบกลับด้วย Pong (0xA)
        if (opcode == 0x09) {
            unsigned char pongHeader[2] = { 0x8A, (unsigned char)payloadLen };
            send(clientSocket, (const char*)pongHeader, 2, 0);
            if (payloadLen > 0) {
                send(clientSocket, payload.data(), (int)payloadLen, 0);
            }
        }

        // หากเป็น Text frame (0x1) ประมวลผลข้อความจาก Browser
        if (opcode == 0x01 && payloadLen > 0) {
            std::string textMsg(payload.data(), payload.size());
            if (m_onMessage) {
                m_onMessage(textMsg, clientSocket);
            }
        }
    }

    RemoveClient(clientSocket);
}

bool WebSocketServer::PerformHandshake(SOCKET clientSocket, const std::string& request) {
    std::string keyHeader = "Sec-WebSocket-Key:";
    size_t keyPos = request.find(keyHeader);
    if (keyPos == std::string::npos) {
        keyHeader = "sec-websocket-key:";
        keyPos = request.find(keyHeader);
    }
    if (keyPos == std::string::npos) return false;

    size_t keyStart = keyPos + keyHeader.size();
    while (keyStart < request.size() && (request[keyStart] == ' ' || request[keyStart] == '\t')) {
        keyStart++;
    }
    size_t keyEnd = request.find("\r\n", keyStart);
    if (keyEnd == std::string::npos) return false;

    std::string clientKey = request.substr(keyStart, keyEnd - keyStart);
    std::string acceptKey = ComputeWebSocketAcceptKey(clientKey);

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << acceptKey << "\r\n\r\n";

    std::string respStr = response.str();
    int sent = send(clientSocket, respStr.data(), (int)respStr.size(), 0);
    return sent > 0;
}

void WebSocketServer::RemoveClient(SOCKET clientSocket) {
    closesocket(clientSocket);
    int currentCount = 0;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        auto it = std::find(m_clients.begin(), m_clients.end(), clientSocket);
        if (it != m_clients.end()) {
            m_clients.erase(it);
        }
        currentCount = (int)m_clients.size();
    }
    if (m_onClientCountChanged) {
        m_onClientCountChanged(currentCount);
    }
}

std::vector<unsigned char> WebSocketServer::EncodeTextFrame(const std::string& textMessage) {
    std::vector<unsigned char> frame;
    frame.push_back(0x81); // FIN + Text frame

    size_t len = textMessage.size();
    if (len <= 125) {
        frame.push_back((unsigned char)len);
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back((unsigned char)((len >> 8) & 0xFF));
        frame.push_back((unsigned char)(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back((unsigned char)((len >> (i * 8)) & 0xFF));
        }
    }

    frame.insert(frame.end(), textMessage.begin(), textMessage.end());
    return frame;
}

void WebSocketServer::SendTo(SOCKET clientSocket, const std::string& textMessage) {
    std::vector<unsigned char> frame = EncodeTextFrame(textMessage);
    send(clientSocket, (const char*)frame.data(), (int)frame.size(), 0);
}

void WebSocketServer::Broadcast(const std::string& textMessage, SOCKET excludeSender) {
    std::vector<unsigned char> frame = EncodeTextFrame(textMessage);

    std::vector<SOCKET> deadClients;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (SOCKET s : m_clients) {
            if (s == excludeSender) continue;
            int sent = send(s, (const char*)frame.data(), (int)frame.size(), 0);
            if (sent == SOCKET_ERROR) {
                deadClients.push_back(s);
            }
        }
        for (SOCKET s : deadClients) {
            closesocket(s);
            auto it = std::find(m_clients.begin(), m_clients.end(), s);
            if (it != m_clients.end()) {
                m_clients.erase(it);
            }
        }
    }

    if (!deadClients.empty() && m_onClientCountChanged) {
        m_onClientCountChanged(GetClientCount());
    }
}

int WebSocketServer::GetClientCount() {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return (int)m_clients.size();
}
