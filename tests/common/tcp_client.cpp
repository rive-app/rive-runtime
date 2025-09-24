/*
 * Copyright 2024 Rive
 */

#include "tcp_client.hpp"

#include "rive/math/math_types.hpp"
#include <stdio.h>
#include <string.h>

#ifndef EXTERNAL_TCP_CLIENT_DEFINITION

std::unique_ptr<TCPClient> TCPClient::Connect(
    const char* serverAddress /*server:port*/)
{
    bool success;
    auto tcpClient =
        std::unique_ptr<TCPClient>(new TCPClient(serverAddress, &success));
    if (success)
    {
        return tcpClient;
    }
    return nullptr;
}

static SOCKET invalid_socket()
{
#ifdef _WIN32
    return INVALID_SOCKET;
#else
    return -1;
#endif
}

static bool is_socket_valid(SOCKET sockfd)
{
#ifdef _WIN32
    return sockfd != INVALID_SOCKET;
#else
    return sockfd > 0;
#endif
}

static void close_socket(SOCKET sockfd)
{
#ifdef __EMSCRIPTEN__
    constexpr static int WEBSOCKET_NORMAL_CLOSURE = 1000;
    emscripten_websocket_close(sockfd, WEBSOCKET_NORMAL_CLOSURE, "finished");
#elif defined(_WIN32)
    closesocket(sockfd);
#else
    close(sockfd);
#endif
}

TCPClient::TCPClient(const char* serverAddress /*server:port*/, bool* success) :
    m_serverAddress(serverAddress), m_sockfd(invalid_socket())
{
    *success = false;

    char hostname[256];
    uint16_t port;
    if (sscanf(serverAddress, "%255[^:]:%hu", hostname, &port) != 2)
    {
        return;
    }

#ifdef __EMSCRIPTEN__
    // WASM has to use websockets instead of TCP.
    if (!emscripten_websocket_is_supported())
    {
        fprintf(stderr,
                "ERROR: WebSockets are not supported in this browser.\n");
        abort();
    }

    auto url = std::string("ws://") + serverAddress;
    EmscriptenWebSocketCreateAttributes attr = {
        .url = url.c_str(),
        .protocols = NULL,
        .createOnMainThread = EM_TRUE,
    };

    m_sockfd = emscripten_websocket_new(&attr);
#else
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup() failed.\n");
        abort();
    }
#endif

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(hostname);
    m_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif

    if (!is_socket_valid(m_sockfd))
    {
        fprintf(stderr,
                "Unable to create socket for %s:%u (%s)\n",
                hostname,
                port,
                strerror(GetLastError()));
        abort();
    }

#ifdef __EMSCRIPTEN__
    emscripten_websocket_set_onopen_callback(m_sockfd, this, OnWebSocketOpen);
    emscripten_websocket_set_onmessage_callback(m_sockfd,
                                                this,
                                                OnWebSocketMessage);
    emscripten_websocket_set_onerror_callback(m_sockfd, this, OnWebSocketError);

    while (m_webSocketStatus != WebSocketStatus::open)
    {
        if (m_webSocketStatus == WebSocketStatus::error)
        {
            fprintf(stderr,
                    "Unable to connect to WebSocket at %s\n",
                    url.c_str());
            abort();
        }
        // Busy wait until our connection is established.
        //
        // NOTE: emscripten_sleep() is an async operation that yields control
        // back to the browser to process.
        emscripten_sleep(10);
    }
#else
    if (connect(m_sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        fprintf(stderr,
                "Unable to connect to TCP server at %s:%u (%s)\n",
                hostname,
                port,
                strerror(GetLastError()));
        abort();
    }
#endif

    *success = true;
}

TCPClient::~TCPClient()
{
    if (is_socket_valid(m_sockfd))
    {
        close_socket(m_sockfd);
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

std::unique_ptr<TCPClient> TCPClient::clone() const
{
    auto clone = TCPClient::Connect(serverAddress());
    if (clone == nullptr)
    {
        fprintf(stderr, "Failed to clone connection to %s\n", serverAddress());
        abort();
    }
    return clone;
}

#ifdef __EMSCRIPTEN__
// Derived from:
// https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c
static std::string base64_encode(const char* buf, unsigned int bufLen)
{
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string ret;
    ret.reserve(((bufLen + 2) / 3) * 4); // 8/6 of the byte length,
                                         // rounded to a multiple of 4.
    int i = 0;
    int j = 0;
    char char_array_3[3];
    char char_array_4[4];

    while (bufLen--)
    {
        char_array_3[i++] = *(buf++);
        if (i == 3)
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                              ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                              ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] =
            ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] =
            ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

uint32_t TCPClient::send(const char* data, uint32_t size)
{
    // Base64-encode our data and send it as text, in order to avoid landmines
    // in binary data transmission.
    std::string base64 = base64_encode(data, size);
    if (emscripten_websocket_send_utf8_text(m_sockfd, base64.c_str()) < 0)
    {
        fprintf(stderr,
                "Failed to send %zu base64 chars to websocket.\n",
                base64.size());
        abort();
    }
    return size;
}
#else
uint32_t TCPClient::send(const char* data, uint32_t size)
{
    size_t sent = ::send(m_sockfd, data, size, 0);
    if (sent == -1)
    {
        fprintf(stderr, "Failed to send %u bytes to server.\n", size);
        abort();
    }
    return rive::math::lossless_numeric_cast<uint32_t>(sent);
}
#endif

uint32_t TCPClient::recv(char* buff, uint32_t size)
{
#ifdef __EMSCRIPTEN__
    while (m_serverMessages.size() < size)
    {
        // Busy wait until the server sends us our data. This isn't ideal, but
        // we always expect immediate responses with our testing protocol.
        //
        // NOTE: emscripten_sleep() is an async operation that yields control
        // back to the browser to process.
        emscripten_sleep(10);
    }
    std::copy(m_serverMessages.begin(), m_serverMessages.begin() + size, buff);
    m_serverMessages.erase(m_serverMessages.begin(),
                           m_serverMessages.begin() + size);
    return size;
#else
    return rive::math::lossless_numeric_cast<uint32_t>(
        ::recv(m_sockfd, buff, size, 0));
#endif
}

void TCPClient::sendall(const void* data, size_t size)
{
    const char* cdata = reinterpret_cast<const char*>(data);
    while (size != 0)
    {
        uint32_t sent = send(cdata,
                             rive::math::lossless_numeric_cast<uint32_t>(
                                 std::min<size_t>(size, 4096)));
        size -= sent;
        cdata += sent;
    }
}

void TCPClient::recvall(void* buff, size_t size)
{
    char* cbuff = reinterpret_cast<char*>(buff);
    while (size != 0)
    {
        uint32_t read = recv(cbuff,
                             rive::math::lossless_numeric_cast<uint32_t>(
                                 std::min<size_t>(size, 4096)));
        size -= read;
        cbuff += read;
    }
}

void TCPClient::send4(uint32_t value)
{
    uint32_t netValue = htonl(value);
    sendall(&netValue, 4);
}

uint32_t TCPClient::recv4()
{
    uint32_t netValue;
    recvall(&netValue, 4);
    return ntohl(netValue);
}

void TCPClient::sendHandshake() { send4(HANDSHAKE_TOKEN); }

void TCPClient::recvHandshake()
{
    uint32_t token = recv4();
    if (token != HANDSHAKE_TOKEN)
    {
        fprintf(stderr, "Bad handshake\n");
        abort();
    }
}

void TCPClient::sendString(const std::string& str)
{
    uint32_t length = rive::math::lossless_numeric_cast<uint32_t>(str.length());
    send4(length);
    send(str.c_str(), length);
}

std::string TCPClient::recvString()
{
    std::string str;
    uint32_t length = recv4();
    str.resize(length);
    recvall(str.data(), length);
    return str;
}

#ifdef __EMSCRIPTEN__
EM_BOOL TCPClient::OnWebSocketOpen(int eventType,
                                   const EmscriptenWebSocketOpenEvent* e,
                                   void* userData)
{
    auto this_ = reinterpret_cast<TCPClient*>(userData);
    assert(e->socket == this_->m_sockfd);
    this_->m_webSocketStatus = WebSocketStatus::open;
    return EM_TRUE;
}

EM_BOOL TCPClient::OnWebSocketMessage(int eventType,
                                      const EmscriptenWebSocketMessageEvent* e,
                                      void* userData)
{
    auto this_ = reinterpret_cast<TCPClient*>(userData);
    assert(e->socket == this_->m_sockfd);
    this_->m_serverMessages.insert(this_->m_serverMessages.end(),
                                   e->data,
                                   e->data + e->numBytes);
    return EM_TRUE;
}

EM_BOOL TCPClient::OnWebSocketError(int eventType,
                                    const EmscriptenWebSocketErrorEvent* e,
                                    void* userData)
{
    auto this_ = reinterpret_cast<TCPClient*>(userData);
    assert(e->socket == this_->m_sockfd);
    this_->m_webSocketStatus = WebSocketStatus::error;
    return EM_TRUE;
}
#endif

#endif
