/*
 * Copyright 2024 Rive
 */

#pragma once

#include <memory>
#include <string>
#include <stdint.h>
#ifndef EXTERNAL_TCP_CLIENT_DEFINITION
#ifdef __EMSCRIPTEN__
#include <emscripten/websocket.h>
#include <arpa/inet.h>
#include <deque>
#define SOCKET EMSCRIPTEN_WEBSOCKET_T
#elif defined(_WIN32)
#include "WinSock2.h"
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif
#endif

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#define SOCKET int
#endif

// Simple utility for connecting a tool to its python harness via TCP socket.
class TCPClient
{
public:
    constexpr static uint32_t HANDSHAKE_TOKEN = 0xfee1600d;
    constexpr static uint32_t SHUTDOWN_TOKEN = 0xfee1dead;

    static std::unique_ptr<TCPClient> Connect(
        const char* serverAddress /*server:port*/);
    ~TCPClient();

    const char* serverAddress() const { return m_serverAddress.c_str(); }
    std::unique_ptr<TCPClient> clone() const;

    void sendall(const void* data, size_t byteCount);
    void recvall(void* data, size_t byteCount);

    // Send a 4-byte uint32_t to the server.
    void send4(uint32_t);

    // Receive a 4-byte uint32_t to the server.
    uint32_t recv4();

    // Send "HANDSHAKE_TOKEN" to the server.
    void sendHandshake();

    // Read 4 bytes from the server and verify they are "HANDSHAKE_TOKEN".
    void recvHandshake();

    void sendString(const std::string& str);

    std::string recvString();

private:
    TCPClient(const char* serverAddress /*server:port*/, bool* success);

    uint32_t send(const char* data, uint32_t size);
    uint32_t recv(char* buff, uint32_t size);

#ifdef _WIN32
    static int GetLastError() { return WSAGetLastError(); }
#elif !defined(NO_RIVE_GET_LAST_ERROR)
    static int GetLastError() { return errno; }
#endif

    const std::string m_serverAddress;
    SOCKET m_sockfd;

#ifdef __EMSCRIPTEN__
    static EM_BOOL OnWebSocketOpen(int eventType,
                                   const EmscriptenWebSocketOpenEvent* e,
                                   void* userData);
    static EM_BOOL OnWebSocketMessage(int eventType,
                                      const EmscriptenWebSocketMessageEvent*,
                                      void* userData);
    static EM_BOOL OnWebSocketError(int eventType,
                                    const EmscriptenWebSocketErrorEvent*,
                                    void* userData);

    enum class WebSocketStatus
    {
        pendingConnection,
        open,
        error,
    };

    WebSocketStatus m_webSocketStatus = WebSocketStatus::pendingConnection;
    std::deque<uint8_t> m_serverMessages;
#endif
};
