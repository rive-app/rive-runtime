/*
 * Copyright 2026 Rive
 */

#ifdef WITH_RIVE_SCRIPTNET

#include "rive/scriptnet/net_policy.hpp"

#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <vector>

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
    bool alphanumeric = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        isDigit(c);
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
  bool hex = lastLabel.size() >= 2 && lastLabel[0] == '0' &&
             lastLabel[1] == 'x';
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

namespace
{
// Where an IPv4 address lies: public, or a kind scripts may not reach.
const char* ipv4Kind(const uint8_t* a)
{
  if (a[0] == 127)
  {
    return "loopback";
  }
  if (a[0] == 10 || (a[0] == 172 && (a[1] & 0xf0) == 16) ||
      (a[0] == 192 && a[1] == 168) || (a[0] == 100 && (a[1] & 0xc0) == 64))
  {
    return "private";
  }
  if (a[0] == 169 && a[1] == 254)
  {
    return "link-local";
  }
  // This network, IETF assignments, documentation (TEST-NET-1 to 3),
  // benchmarking, multicast, and the reserved block with broadcast.
  if (a[0] == 0 || (a[0] == 192 && a[1] == 0 && (a[2] == 0 || a[2] == 2)) ||
      (a[0] == 198 && a[1] == 51 && a[2] == 100) ||
      (a[0] == 203 && a[1] == 0 && a[2] == 113) ||
      (a[0] == 198 && (a[1] & 0xfe) == 18) || a[0] >= 224)
  {
    return "reserved";
  }
  return nullptr;
}

bool isZero(const uint8_t* bytes, size_t length)
{
  for (size_t i = 0; i < length; i++)
  {
    if (bytes[i] != 0)
    {
      return false;
    }
  }
  return true;
}

const char* ipv6Kind(const uint8_t* a)
{
  if (isZero(a, 15))
  {
    return a[15] == 0 ? "reserved" : a[15] == 1 ? "loopback" : nullptr;
  }
  // IPv4-mapped (::ffff:0:0/96) and the deprecated IPv4-compatible (::/96)
  // forms carry the address in the last four bytes, as does NAT64's
  // well-known prefix (64:ff9b::/96); 6to4 (2002::/16) carries it after
  // the prefix.
  if (isZero(a, 10) &&
      ((a[10] == 0xff && a[11] == 0xff) || (a[10] == 0 && a[11] == 0)))
  {
    return ipv4Kind(a + 12);
  }
  if (a[0] == 0 && a[1] == 0x64 && a[2] == 0xff && a[3] == 0x9b)
  {
    if (a[4] == 0 && a[5] == 1)
    {
      // Local-use NAT64 (64:ff9b:1::/48).
      return "private";
    }
    return isZero(a + 4, 8) ? ipv4Kind(a + 12) : nullptr;
  }
  if (a[0] == 0x20 && a[1] == 0x02)
  {
    return ipv4Kind(a + 2);
  }
  if ((a[0] & 0xfe) == 0xfc || (a[0] == 0xfe && (a[1] & 0xc0) == 0xc0))
  {
    // Unique local (fc00::/7) and the deprecated site-local (fec0::/10).
    return "private";
  }
  if (a[0] == 0xfe && (a[1] & 0xc0) == 0x80)
  {
    return "link-local";
  }
  // Multicast, the discard prefix (100::/64) and documentation
  // (2001:db8::/32).
  if (a[0] == 0xff || (a[0] == 1 && isZero(a + 1, 7)) ||
      (a[0] == 0x20 && a[1] == 0x01 && a[2] == 0x0d && a[3] == 0xb8))
  {
    return "reserved";
  }
  return nullptr;
}
} // namespace

bool NetPolicy::checkAddress(
    const uint8_t* address,
    size_t length,
    std::string* reason)
{
  if (length != 4 && length != 16)
  {
    if (reason != nullptr)
    {
      *reason = "the host resolves to an address of an unknown kind";
    }
    return false;
  }
  const char* kind = length == 4 ? ipv4Kind(address) : ipv6Kind(address);
  if (kind == nullptr)
  {
    return true;
  }
  if (reason != nullptr)
  {
    *reason = std::string("the host resolves to a ") + kind + " address";
  }
  return false;
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

bool NetPolicy::check(
    HttpRequest& request,
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
    return refuse(
        error,
        "the URL is longer than " + std::to_string(limits.maxUrlLength) +
            " characters");
  }
  for (char c : url)
  {
    unsigned char byte = (unsigned char)c;
    if (byte <= 0x20 || byte >= 0x7f || c == '\\')
    {
      return refuse(
          error,
          "the URL must be ASCII with no spaces or "
          "backslashes; percent-encode anything else");
    }
  }
  size_t colon = url.find(':');
  std::string scheme = colon == std::string::npos
                           ? ""
                           : lowerAscii(url.substr(0, colon));
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
  std::string authority = url.substr(
      authorityStart,
      authorityEnd - authorityStart);
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
  std::string rest = url.substr(
      authorityEnd,
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
  std::string method = request.method.empty() ? std::string("GET")
                                              : toUpper(request.method);
  bool allowed = false;
  for (const char* candidate : kAllowedMethods)
  {
    allowed = allowed || method == candidate;
  }
  if (!allowed)
  {
    return refuse(
        error,
        isToken(method) ? "the " + method + " method is not allowed"
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
    return refuse(
        error,
        "more than " + std::to_string(limits.maxHeaderCount) + " headers");
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
        return refuse(
            error,
            "the value of the '" + header.name +
                "' header must be printable ASCII");
      }
    }
    if (isForbiddenHeader(header.name))
    {
      return refuse(
          error,
          "scripts cannot set the '" + header.name + "' header");
    }
    headerBytes += header.name.size() + header.value.size();
  }
  if (headerBytes > limits.maxHeaderBytes)
  {
    return refuse(
        error,
        "the headers are larger than " + std::to_string(limits.maxHeaderBytes) +
            " bytes");
  }

  // Body and timeout.
  if (request.body.size() > limits.maxRequestBodyBytes)
  {
    return refuse(
        error,
        "the request body is larger than " +
            std::to_string(limits.maxRequestBodyBytes) + " bytes");
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

namespace
{
// RFC 3986 section 5.2.4, on a path that starts with '/'.
std::string removeDotSegments(const std::string& path)
{
  std::vector<std::string> kept;
  bool directory = false;
  size_t start = 1;
  while (start <= path.size())
  {
    size_t slash = path.find('/', start);
    bool last = slash == std::string::npos;
    size_t end = last ? path.size() : slash;
    std::string segment = path.substr(start, end - start);
    if (segment == ".")
    {
      directory = last;
    }
    else if (segment == "..")
    {
      if (!kept.empty())
      {
        kept.pop_back();
      }
      directory = last;
    }
    else
    {
      kept.push_back(std::move(segment));
      directory = false;
    }
    if (last)
    {
      break;
    }
    start = end + 1;
  }
  std::string result;
  for (const std::string& segment : kept)
  {
    result += '/' + segment;
  }
  if (directory)
  {
    result += '/';
  }
  return result.empty() ? std::string("/") : result;
}

bool isSchemeChar(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || isDigit(c) ||
         c == '+' || c == '-' || c == '.';
}

// RFC 3986 section 5.2: `location` against `base`, a URL check() let
// through. The fragment is dropped, as check() drops it.
bool resolveLocation(
    const std::string& base,
    std::string location,
    std::string* resolved)
{
  size_t first = 0;
  while (first < location.size() &&
         (location[first] == ' ' || location[first] == '\t'))
  {
    first++;
  }
  location.erase(0, first);
  while (!location.empty() &&
         (location.back() == ' ' || location.back() == '\t'))
  {
    location.pop_back();
  }
  if (location.empty())
  {
    return false;
  }
  // Servers send raw UTF-8, spaces and the odd control character in a
  // Location, which browsers percent-encode before using it, as does
  // NSURLSession before the Apple transport sees a hop. The same here, so
  // the hop can pass check(), which takes nothing but ASCII.
  std::string encoded;
  for (char c : location)
  {
    unsigned char byte = (unsigned char)c;
    if (byte <= 0x20 || byte >= 0x7f || c == '\\')
    {
      static const char kHex[] = "0123456789ABCDEF";
      encoded += '%';
      encoded += kHex[byte >> 4];
      encoded += kHex[byte & 0xf];
    }
    else
    {
      encoded += c;
    }
  }
  location = std::move(encoded);
  // Absolute, with a scheme of its own.

  if ((location[0] >= 'a' && location[0] <= 'z') ||
      (location[0] >= 'A' && location[0] <= 'Z'))
  {
    size_t at = 1;
    while (at < location.size() && isSchemeChar(location[at]))
    {
      at++;
    }
    if (at < location.size() && location[at] == ':')
    {
      *resolved = location;
      return true;
    }
  }
  size_t authorityStart = base.find("://");
  if (authorityStart == std::string::npos)
  {
    return false;
  }
  authorityStart += 3;
  if (location.compare(0, 2, "//") == 0)
  {
    *resolved = base.substr(0, authorityStart - 2) + location;
    return true;
  }
  size_t authorityEnd = base.find_first_of("/?#", authorityStart);
  if (authorityEnd == std::string::npos)
  {
    authorityEnd = base.size();
  }
  std::string origin = base.substr(0, authorityEnd);
  size_t pathEnd = base.find_first_of("?#", authorityEnd);
  std::string path = base.substr(
      authorityEnd,
      pathEnd == std::string::npos ? std::string::npos
                                   : pathEnd - authorityEnd);
  if (path.empty())
  {
    path = "/";
  }
  if (location[0] == '#')
  {
    // The same resource, query included; the fragment never reaches it.
    *resolved = base;
    return true;
  }
  std::string merged;
  if (location[0] == '/')
  {
    merged = location;
  }
  else if (location[0] == '?')
  {
    merged = path + location;
  }
  else
  {
    merged = path.substr(0, path.rfind('/') + 1) + location;
  }
  size_t mergedPathEnd = merged.find_first_of("?#");
  std::string tail = mergedPathEnd == std::string::npos
                         ? std::string()
                         : merged.substr(mergedPathEnd);
  *resolved = origin + removeDotSegments(merged.substr(0, mergedPathEnd)) +
              tail;
  return true;
}

// scheme://host[:port] of a URL check() let through, as an origin
// compares: the port by value, since check() keeps it as written, and
// left off when it is the default for https:.
std::string originOf(const std::string& url)
{
  size_t authorityStart = url.find("://");
  authorityStart = authorityStart == std::string::npos ? 0 : authorityStart + 3;
  std::string origin = url.substr(0, url.find_first_of("/?#", authorityStart));
  size_t colon = origin.find(':', authorityStart);
  if (colon == std::string::npos)
  {
    return origin;
  }
  uint32_t port = 0;
  for (size_t at = colon + 1; at < origin.size(); at++)
  {
    port = port * 10 + (uint32_t)(origin[at] - '0');
  }
  origin.resize(colon);
  if (port != 443)
  {
    origin += ':' + std::to_string(port);
  }
  return origin;
}

void dropHeaders(HttpHeaders* headers, std::initializer_list<const char*> names)
{
  for (size_t i = headers->size(); i-- > 0;)
  {
    std::string name = lowerAscii((*headers)[i].name);
    for (const char* dropped : names)
    {
      if (name == dropped)
      {
        headers->erase(headers->begin() + (std::ptrdiff_t)i);
        break;
      }
    }
  }
}
} // namespace

bool NetPolicy::redirect(
    HttpRequest& request,
    uint16_t status,
    const std::string& location,
    uint32_t hops,
    const NetLimits& limits,
    NetError* error)
{
  if (hops >= limits.maxRedirects)
  {
    if (error != nullptr)
    {
      error->code = NetErrorCode::Network;
      error->message = "more than " + std::to_string(limits.maxRedirects) +
                       " redirects";
    }
    return false;
  }
  HttpRequest hop = request;
  if (!resolveLocation(request.url, location, &hop.url))
  {
    return refuse(error, "the Location header could not be resolved");
  }
  // The Fetch standard's HTTP-redirect fetch: these continue as a GET,
  // without the body and the headers that described it.
  if ((status == 303 && hop.method != "GET" && hop.method != "HEAD") ||
      ((status == 301 || status == 302) && hop.method == "POST"))
  {
    hop.method = "GET";
    hop.body.clear();
    dropHeaders(
        &hop.headers,
        {"content-encoding",
         "content-language",
         "content-location",
         "content-type"});
  }
  if (!check(hop, limits, error))
  {
    return false;
  }
  // Credentials stay with the origin they were meant for.
  if (originOf(hop.url) != originOf(request.url))
  {
    dropHeaders(&hop.headers, {"authorization"});
  }
  request = std::move(hop);
  return true;
}

} // namespace rive::scriptnet

#endif // WITH_RIVE_SCRIPTNET
