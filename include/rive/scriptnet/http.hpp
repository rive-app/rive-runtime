/*
 * Copyright 2026 Rive
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rive::scriptnet
{

/// One header line. Order and duplicates are kept; names compare
/// case-insensitively.
struct HttpHeader
{
    std::string name;
    std::string value;
};
using HttpHeaders = std::vector<HttpHeader>;

/// A request a script asked for, before and after NetPolicy::check
/// normalizes it.
struct HttpRequest
{
    std::string url;
    std::string method = "GET";
    HttpHeaders headers;
    std::vector<uint8_t> body;
    /// Zero means the policy default.
    uint32_t timeoutMs = 0;
};

struct HttpResponse
{
    uint16_t status = 0;
    std::string statusText;
    /// The final URL, after redirects.
    std::string url;
    HttpHeaders headers;
    std::vector<uint8_t> body;
};

enum class NetErrorCode : uint8_t
{
    /// The host has not given scripts a network transport.
    Disabled,
    /// NetPolicy refused the request: scheme, host, method, header or size.
    Policy,
    /// Too many requests in flight, or too many in the last minute.
    RateLimited,
    /// No response within the timeout.
    Timeout,
    /// Cancelled by the script, or its VM shut down.
    Aborted,
    /// The response body exceeded the size limit.
    TooLarge,
    /// DNS, TLS, CORS, connection or any other transport failure.
    Network,
};

struct NetError
{
    NetErrorCode code = NetErrorCode::Network;
    std::string message;
};

/// Header names and hosts are ASCII case-insensitive.
std::string lowerAscii(std::string value);

/// Stable lower-case name for `code` ("disabled", "policy", ...). Scripts
/// see it at the start of every rejection: "<name>: <message>".
const char* netErrorCodeName(NetErrorCode code);

/// Inverse of netErrorCodeName; unknown names map to Network.
NetErrorCode netErrorCodeFromName(const char* name, size_t length);

} // namespace rive::scriptnet
