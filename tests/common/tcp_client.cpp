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
#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
}

TCPClient::TCPClient(const char* serverAddress /*server:port*/, bool* success) :
    m_serverAddress(serverAddress)
{
    *success = false;

    m_sockfd = invalid_socket();

    char hostname[256];
    uint16_t port;
    if (sscanf(serverAddress, "%255[^:]:%hu", hostname, &port) != 2)
    {
        return;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup() failed.\n");
        abort();
        return;
    }
#endif

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(hostname);
    m_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (!is_socket_valid(m_sockfd))
    {
        fprintf(stderr,
                "Unable to create socket for %s:%u (%s)\n",
                hostname,
                port,
                strerror(GetLastError()));
        abort();
    }

    if (connect(m_sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        fprintf(stderr,
                "Unable to connect to TCP server at %s:%u (%s)\n",
                hostname,
                port,
                strerror(GetLastError()));
        abort();
    }

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

uint32_t TCPClient::recv(char* buff, uint32_t size)
{
    return rive::math::lossless_numeric_cast<uint32_t>(
        ::recv(m_sockfd, buff, size, 0));
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
#endif
