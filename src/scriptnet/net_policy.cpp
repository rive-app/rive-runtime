/*
 * Copyright 2026 Rive
 */

#ifdef WITH_RIVE_SCRIPTNET

#include "rive/scriptnet/net_policy.hpp"

#include <cstring>

namespace rive::scriptnet
{
namespace
{
// Hosts Rive operates. Script requests carry no cookies, but these must not
// see traffic from untrusted files at all: our own CORS allowlists, quotas
// and origin checks trust requests that look like they come from us.
const char* const kRiveHosts[] = {
    "rive.app",
    "2dimensions.com",
    "riveusercontent.com",
};

// Names that only resolve on a local network.
const char* const kLocalHosts[] = {
    "localhost",
    "local",
    "internal",
    "home.arpa",
    "localdomain",
    "lan",
};

// https://fetch.spec.whatwg.org/#forbidden-request-header, plus the method
// override family, which the standard only forbids alongside a forbidden
// method.
const char* const kForbiddenHeaders[] = {
    "accept-charset",
    "accept-encoding",
    "access-control-request-headers",
    "access-control-request-method",
    "access-control-request-private-network",
    "connection",
    "content-length",
    "cookie",
    "cookie2",
    "date",
    "dnt",
    "expect",
    "host",
    "keep-alive",
    "origin",
    "referer",
    "set-cookie",
    "te",
    "trailer",
    "transfer-encoding",
    "upgrade",
    "via",
    "x-http-method",
    "x-http-method-override",
    "x-method-override",
};

const char* const kAllowedMethods[] = {
    "GET",
    "HEAD",
    "POST",
    "PUT",
    "PATCH",
    "DELETE",
    "OPTIONS",
};

bool isDigit(char c) { return c >= '0' && c <= '9'; }

bool isHexDigit(char c)
{
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

std::string toUpper(std::string value)
{
    for (char& c : value)
    {
        if (c >= 'a' && c <= 'z')
        {
            c = (char)(c - 'a' + 'A');
        }
    }
    return value;
}

// RFC 9110 token characters, which methods and header names are made of.
bool isToken(const std::string& value)
{
    if (value.empty())
    {
        return false;
    }
    for (char c : value)
    {
        bool alphanumeric =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || isDigit(c);
        if (!alphanumeric &&
            (c == '\0' || std::strchr("!#$%&'*+-.^_`|~", c) == nullptr))
        {
            return false;
        }
    }
    return true;
}

// True when `host` is `suffix` or a subdomain of it.
bool isWithin(const std::string& host, const char* suffix)
{
    size_t length = std::strlen(suffix);
    if (host.size() < length ||
        host.compare(host.size() - length, length, suffix) != 0)
    {
        return false;
    }
    return host.size() == length || host[host.size() - length - 1] == '.';
}

bool refuse(NetError* error, std::string message)
{
    if (error != nullptr)
    {
        error->code = NetErrorCode::Policy;
        error->message = std::move(message);
    }
    return false;
}
} // namespace

std::string lowerAscii(std::string value)
{
    for (char& c : value)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c = (char)(c - 'A' + 'a');
        }
    }
    return value;
}

bool NetPolicy::checkHost(const std::string& host, std::string* reason)
{
    auto refuseHost = [reason](const char* why) {
        if (reason != nullptr)
        {
            *reason = why;
        }
        return false;
    };
    if (host.empty())
    {
        return refuseHost("the URL has no host");
    }
    if (host.size() > 253)
    {
        return refuseHost("the host name is too long");
    }
    for (char c : host)
    {
        if (c == '[' || c == ']' || c == ':')
        {
            return refuseHost("IP address hosts are not allowed");
        }
        if (!((c >= 'a' && c <= 'z') || isDigit(c) || c == '-' || c == '.' ||
              c == '_'))
        {
            return refuseHost("the host must be an ASCII domain name "
                              "(punycode for international names)");
        }
    }

    size_t labels = 0;
    size_t lastLabelStart = 0;
    size_t start = 0;
    while (true)
    {
        size_t dot = host.find('.', start);
        size_t end = dot == std::string::npos ? host.size() : dot;
        if (end == start)
        {
            return refuseHost("the host has an empty label");
        }
        if (end - start > 63)
        {
            return refuseHost("a host label is longer than 63 characters");
        }
        labels++;
        lastLabelStart = start;
        if (dot == std::string::npos)
        {
            break;
        }
        start = dot + 1;
    }

    // The URL standard parses a host whose last label is a number as IPv4, so
    // 127.1, 0x7f.1 and 2130706433 all reach loopback. Refuse them all.
    std::string lastLabel = host.substr(lastLabelStart);
    bool decimal = true;
    for (char c : lastLabel)
    {
        decimal = decimal && isDigit(c);
    }
    bool hex =
        lastLabel.size() >= 2 && lastLabel[0] == '0' && lastLabel[1] == 'x';
    for (size_t i = 2; hex && i < lastLabel.size(); i++)
    {
        hex = isHexDigit(lastLabel[i]);
    }
    if (decimal || hex)
    {
        return refuseHost("IP address hosts are not allowed");
    }

    if (labels < 2)
    {
        return refuseHost("the host must be a fully qualified domain name");
    }
    for (const char* local : kLocalHosts)
    {
        if (isWithin(host, local))
        {
            return refuseHost("local network hosts are not allowed");
        }
    }
    for (const char* rive : kRiveHosts)
    {
        if (isWithin(host, rive))
        {
            return refuseHost("Rive's own hosts are not reachable from "
                              "scripts");
        }
    }
    return true;
}

bool NetPolicy::isForbiddenHeader(const std::string& name)
{
    std::string lower = lowerAscii(name);
    if (lower.compare(0, 6, "proxy-") == 0 || lower.compare(0, 4, "sec-") == 0)
    {
        return true;
    }
    for (const char* forbidden : kForbiddenHeaders)
    {
        if (lower == forbidden)
        {
            return true;
        }
    }
    return false;
}

bool NetPolicy::check(HttpRequest& request,
                      const NetLimits& limits,
                      NetError* error)
{
    // URL. Only a conservative subset parses here, since the transport
    // parses the URL again with its own parser.
    const std::string& url = request.url;
    if (url.empty())
    {
        return refuse(error, "the URL is empty");
    }
    if (url.size() > limits.maxUrlLength)
    {
        return refuse(error,
                      "the URL is longer than " +
                          std::to_string(limits.maxUrlLength) + " characters");
    }
    for (char c : url)
    {
        unsigned char byte = (unsigned char)c;
        if (byte <= 0x20 || byte >= 0x7f || c == '\\')
        {
            return refuse(error,
                          "the URL must be ASCII with no spaces or "
                          "backslashes; percent-encode anything else");
        }
    }
    size_t colon = url.find(':');
    std::string scheme =
        colon == std::string::npos ? "" : lowerAscii(url.substr(0, colon));
    if (scheme == "http")
    {
        return refuse(error, "http: URLs are not allowed; use https:");
    }
    if (scheme != "https")
    {
        return refuse(error, "only https: URLs are allowed");
    }
    if (url.compare(colon + 1, 2, "//") != 0)
    {
        return refuse(error, "the URL has no host");
    }
    size_t authorityStart = colon + 3;
    size_t authorityEnd = url.find_first_of("/?#", authorityStart);
    if (authorityEnd == std::string::npos)
    {
        authorityEnd = url.size();
    }
    std::string authority =
        url.substr(authorityStart, authorityEnd - authorityStart);
    if (authority.find('@') != std::string::npos)
    {
        return refuse(error, "URLs may not carry credentials");
    }
    if (!authority.empty() && authority[0] == '[')
    {
        return refuse(error, "IP address hosts are not allowed");
    }
    std::string host = authority;
    std::string port;
    size_t portColon = authority.rfind(':');
    if (portColon != std::string::npos)
    {
        host = authority.substr(0, portColon);
        port = authority.substr(portColon + 1);
        uint32_t portValue = 0;
        for (char c : port)
        {
            portValue = isDigit(c) && portValue <= 65535
                            ? portValue * 10 + (uint32_t)(c - '0')
                            : 65536;
        }
        if (!port.empty() && (portValue == 0 || portValue > 65535))
        {
            return refuse(error, "the URL has an invalid port");
        }
    }
    host = lowerAscii(host);
    if (!host.empty() && host.back() == '.')
    {
        host.pop_back();
    }
    std::string reason;
    if (!checkHost(host, &reason))
    {
        return refuse(error, reason);
    }
    // The fragment never reaches a server.
    size_t fragment = url.find('#', authorityEnd);
    std::string rest =
        url.substr(authorityEnd,
                   fragment == std::string::npos ? std::string::npos
                                                 : fragment - authorityEnd);
    std::string normalized = "https://" + host;
    if (!port.empty())
    {
        normalized += ':' + port;
    }
    normalized += rest;
    request.url = std::move(normalized);

    // Method.
    std::string method =
        request.method.empty() ? std::string("GET") : toUpper(request.method);
    bool allowed = false;
    for (const char* candidate : kAllowedMethods)
    {
        allowed = allowed || method == candidate;
    }
    if (!allowed)
    {
        return refuse(error,
                      isToken(method)
                          ? "the " + method + " method is not allowed"
                          : std::string("the method is not a valid HTTP "
                                        "token"));
    }
    request.method = method;
    if ((method == "GET" || method == "HEAD") && !request.body.empty())
    {
        return refuse(error, "GET and HEAD requests cannot have a body");
    }

    // Headers.
    if (request.headers.size() > limits.maxHeaderCount)
    {
        return refuse(error,
                      "more than " + std::to_string(limits.maxHeaderCount) +
                          " headers");
    }
    size_t headerBytes = 0;
    for (const HttpHeader& header : request.headers)
    {
        if (!isToken(header.name))
        {
            return refuse(error, "a header name is not a valid HTTP token");
        }
        for (char c : header.value)
        {
            unsigned char byte = (unsigned char)c;
            if ((byte < 0x20 && c != '\t') || byte >= 0x7f)
            {
                return refuse(error,
                              "the value of the '" + header.name +
                                  "' header must be printable ASCII");
            }
        }
        if (isForbiddenHeader(header.name))
        {
            return refuse(error,
                          "scripts cannot set the '" + header.name +
                              "' header");
        }
        headerBytes += header.name.size() + header.value.size();
    }
    if (headerBytes > limits.maxHeaderBytes)
    {
        return refuse(error,
                      "the headers are larger than " +
                          std::to_string(limits.maxHeaderBytes) + " bytes");
    }

    // Body and timeout.
    if (request.body.size() > limits.maxRequestBodyBytes)
    {
        return refuse(error,
                      "the request body is larger than " +
                          std::to_string(limits.maxRequestBodyBytes) +
                          " bytes");
    }
    if (request.timeoutMs == 0)
    {
        request.timeoutMs = limits.defaultTimeoutMs;
    }
    if (request.timeoutMs > limits.maxTimeoutMs)
    {
        request.timeoutMs = limits.maxTimeoutMs;
    }
    return true;
}

} // namespace rive::scriptnet

#endif // WITH_RIVE_SCRIPTNET
