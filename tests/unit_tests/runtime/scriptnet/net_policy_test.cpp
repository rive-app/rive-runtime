#ifdef WITH_RIVE_SCRIPTNET

#include "catch.hpp"
#include "rive/scriptnet/net_policy.hpp"

#include <initializer_list>
#include <string>
#include <vector>

using namespace rive::scriptnet;

namespace
{
// The policy's refusal message, or empty when it lets the request through.
std::string refusal(HttpRequest& request, const NetLimits& limits = NetLimits())
{
  NetError error;
  if (NetPolicy::check(request, limits, &error))
  {
    return "";
  }
  CHECK(error.code == NetErrorCode::Policy);
  return error.message;
}

std::string urlRefusal(const std::string& url)
{
  HttpRequest request;
  request.url = url;
  return refusal(request);
}

std::string normalizedUrl(const std::string& url)
{
  HttpRequest request;
  request.url = url;
  REQUIRE(refusal(request).empty());
  return request.url;
}

const char* kIpHost = "IP address hosts are not allowed";
const char* kLocalHost = "local network hosts are not allowed";
const char* kRiveHost = "Rive's own hosts are not reachable from scripts";
} // namespace

TEST_CASE(
    "NetPolicy lets https requests to public hosts through",
    "[scriptnet]")
{
  CHECK(urlRefusal("https://example.com/").empty());
  CHECK(urlRefusal("https://api.example.com:8443/v1/items?limit=5").empty());
  CHECK(urlRefusal("https://xn--bcher-kva.example/").empty());
  CHECK(urlRefusal("https://example.com./").empty());
  CHECK(urlRefusal("https://my_service.example.com/").empty());
  // Look-alikes of refused hosts are not the hosts themselves.
  CHECK(urlRefusal("https://notrive.app/").empty());
  CHECK(urlRefusal("https://rive.app.example.com/").empty());
  CHECK(urlRefusal("https://localhost.example.com/").empty());
}

TEST_CASE("NetPolicy normalizes the URL it lets through", "[scriptnet]")
{
  CHECK(
      normalizedUrl("HTTPS://Example.COM/Path?Q=1") ==
      "https://example.com/Path?Q=1");
  CHECK(
      normalizedUrl("https://example.com/page#section") ==
      "https://example.com/page");
  CHECK(
      normalizedUrl("https://example.com.:443/") == "https://example.com:443/");
  CHECK(normalizedUrl("https://example.com:/") == "https://example.com/");
  CHECK(normalizedUrl("https://example.com?x") == "https://example.com?x");
}

TEST_CASE("NetPolicy refuses every scheme but https", "[scriptnet]")
{
  CHECK(
      urlRefusal("http://example.com/") ==
      "http: URLs are not allowed; use https:");
  CHECK(
      urlRefusal("HTTP://example.com/") ==
      "http: URLs are not allowed; use https:");
  CHECK(urlRefusal("ftp://example.com/") == "only https: URLs are allowed");
  CHECK(urlRefusal("wss://example.com/") == "only https: URLs are allowed");
  CHECK(urlRefusal("file:///etc/passwd") == "only https: URLs are allowed");
  CHECK(urlRefusal("example.com") == "only https: URLs are allowed");
  CHECK(urlRefusal("https:example.com") == "the URL has no host");
  CHECK(urlRefusal("https:///path") == "the URL has no host");
  CHECK(urlRefusal("") == "the URL is empty");
}

TEST_CASE("NetPolicy refuses URLs it cannot parse unambiguously", "[scriptnet]")
{
  const char* ascii = "the URL must be ASCII with no spaces or backslashes; "
                      "percent-encode anything else";
  CHECK(urlRefusal("https://example.com/a b") == ascii);
  CHECK(urlRefusal("https://example.com\\@evil.example/") == ascii);
  CHECK(urlRefusal("https://ex\xC3\xA4mple.com/") == ascii);
  CHECK(urlRefusal("https://example.com/\n") == ascii);
  CHECK(
      urlRefusal("https://user:secret@example.com/") ==
      "URLs may not carry credentials");
  CHECK(
      urlRefusal("https://user@example.com/") ==
      "URLs may not carry credentials");
  CHECK(urlRefusal("https://example.com:0/") == "the URL has an invalid port");
  CHECK(
      urlRefusal("https://example.com:65536/") ==
      "the URL has an invalid port");
  CHECK(
      urlRefusal("https://example.com:44a/") == "the URL has an invalid port");
  CHECK(urlRefusal("https://example..com/") == "the host has an empty label");
  CHECK(urlRefusal("https://.example.com/") == "the host has an empty label");
  CHECK(
      urlRefusal("https://exa%6Dple.com/") ==
      "the host must be an ASCII domain name (punycode for international "
      "names)");
}

TEST_CASE(
    "NetPolicy refuses IP literals in every form the URL standard reads",
    "[scriptnet]")
{
  CHECK(urlRefusal("https://127.0.0.1/") == kIpHost);
  CHECK(urlRefusal("https://1.2.3.4/") == kIpHost);
  CHECK(urlRefusal("https://127.1/") == kIpHost);
  CHECK(urlRefusal("https://0x7f.1/") == kIpHost);
  CHECK(urlRefusal("https://0x7f000001/") == kIpHost);
  CHECK(urlRefusal("https://2130706433/") == kIpHost);
  CHECK(urlRefusal("https://192.168.0.1./") == kIpHost);
  CHECK(urlRefusal("https://[::1]/") == kIpHost);
  CHECK(urlRefusal("https://[2001:db8::1]:443/") == kIpHost);
}

TEST_CASE(
    "NetPolicy refuses local network and single-label hosts",
    "[scriptnet]")
{
  const char* unqualified = "the host must be a fully qualified domain name";
  CHECK(urlRefusal("https://localhost/") == unqualified);
  CHECK(urlRefusal("https://intranet/") == unqualified);
  CHECK(urlRefusal("https://app.localhost/") == kLocalHost);
  CHECK(urlRefusal("https://printer.local/") == kLocalHost);
  CHECK(urlRefusal("https://db.internal/") == kLocalHost);
  CHECK(urlRefusal("https://router.home.arpa/") == kLocalHost);
  CHECK(urlRefusal("https://nas.lan/") == kLocalHost);
}

// The reason checkAddress() gives for an address, or "" when it passes.
std::string addressRefusal(std::initializer_list<int> bytes)
{
  std::vector<uint8_t> address;
  for (int byte : bytes)
  {
    address.push_back((uint8_t)byte);
  }
  std::string reason;
  if (NetPolicy::checkAddress(address.data(), address.size(), &reason))
  {
    return "";
  }
  return reason;
}

// An IPv6 address from its eight groups.
std::string v6Refusal(std::initializer_list<int> groups)
{
  std::vector<uint8_t> address;
  for (int group : groups)
  {
    address.push_back((uint8_t)(group >> 8));
    address.push_back((uint8_t)group);
  }
  std::string reason;
  if (NetPolicy::checkAddress(address.data(), address.size(), &reason))
  {
    return "";
  }
  return reason;
}

const char* kLoopback = "the host resolves to a loopback address";
const char* kPrivate = "the host resolves to a private address";
const char* kLinkLocal = "the host resolves to a link-local address";
const char* kReserved = "the host resolves to a reserved address";

TEST_CASE("NetPolicy lets public IPv4 addresses through", "[scriptnet]")
{
  CHECK(addressRefusal({93, 184, 216, 34}) == "");
  CHECK(addressRefusal({1, 1, 1, 1}) == "");
  CHECK(addressRefusal({8, 8, 8, 8}) == "");
  // The neighbours of the refused blocks.
  CHECK(addressRefusal({11, 0, 0, 1}) == "");
  CHECK(addressRefusal({172, 15, 255, 255}) == "");
  CHECK(addressRefusal({172, 32, 0, 1}) == "");
  CHECK(addressRefusal({192, 167, 255, 255}) == "");
  CHECK(addressRefusal({192, 169, 0, 1}) == "");
  CHECK(addressRefusal({100, 63, 255, 255}) == "");
  CHECK(addressRefusal({100, 128, 0, 1}) == "");
  CHECK(addressRefusal({169, 253, 0, 1}) == "");
  CHECK(addressRefusal({192, 0, 1, 1}) == "");
  CHECK(addressRefusal({192, 0, 3, 1}) == "");
  CHECK(addressRefusal({198, 17, 255, 255}) == "");
  CHECK(addressRefusal({198, 20, 0, 1}) == "");
  CHECK(addressRefusal({223, 255, 255, 255}) == "");
}

TEST_CASE(
    "NetPolicy refuses local, private and reserved IPv4 addresses",
    "[scriptnet]")
{
  CHECK(addressRefusal({127, 0, 0, 1}) == kLoopback);
  CHECK(addressRefusal({127, 255, 255, 255}) == kLoopback);
  CHECK(addressRefusal({10, 0, 0, 1}) == kPrivate);
  CHECK(addressRefusal({172, 16, 0, 1}) == kPrivate);
  CHECK(addressRefusal({172, 31, 255, 255}) == kPrivate);
  CHECK(addressRefusal({192, 168, 1, 1}) == kPrivate);
  // Carrier-grade NAT's shared space.
  CHECK(addressRefusal({100, 64, 0, 1}) == kPrivate);
  CHECK(addressRefusal({100, 127, 255, 255}) == kPrivate);
  // Where cloud instance metadata lives.
  CHECK(addressRefusal({169, 254, 169, 254}) == kLinkLocal);
  CHECK(addressRefusal({0, 0, 0, 0}) == kReserved);
  CHECK(addressRefusal({0, 1, 2, 3}) == kReserved);
  CHECK(addressRefusal({192, 0, 0, 1}) == kReserved);
  CHECK(addressRefusal({192, 0, 2, 1}) == kReserved);
  CHECK(addressRefusal({198, 51, 100, 1}) == kReserved);
  CHECK(addressRefusal({203, 0, 113, 1}) == kReserved);
  CHECK(addressRefusal({198, 18, 0, 1}) == kReserved);
  CHECK(addressRefusal({198, 19, 255, 255}) == kReserved);
  CHECK(addressRefusal({224, 0, 0, 1}) == kReserved);
  CHECK(addressRefusal({240, 0, 0, 1}) == kReserved);
  CHECK(addressRefusal({255, 255, 255, 255}) == kReserved);
}

TEST_CASE("NetPolicy judges IPv6 addresses", "[scriptnet]")
{
  CHECK(v6Refusal({0x2606, 0x4700, 0, 0, 0, 0, 0, 1}) == "");
  CHECK(v6Refusal({0x2001, 0x4860, 0x4860, 0, 0, 0, 0, 0x8888}) == "");
  CHECK(v6Refusal({0x2001, 0x0db9, 0, 0, 0, 0, 0, 1}) == "");
  CHECK(v6Refusal({0, 0, 0, 0, 0, 0, 0, 1}) == kLoopback);
  CHECK(v6Refusal({0, 0, 0, 0, 0, 0, 0, 0}) == kReserved);
  CHECK(v6Refusal({0xfe80, 0, 0, 0, 0, 0, 0, 1}) == kLinkLocal);
  CHECK(v6Refusal({0xfebf, 0, 0, 0, 0, 0, 0, 1}) == kLinkLocal);
  CHECK(v6Refusal({0xfc00, 0, 0, 0, 0, 0, 0, 1}) == kPrivate);
  CHECK(v6Refusal({0xfd12, 0x3456, 0, 0, 0, 0, 0, 1}) == kPrivate);
  CHECK(v6Refusal({0xfec0, 0, 0, 0, 0, 0, 0, 1}) == kPrivate);
  CHECK(v6Refusal({0xff02, 0, 0, 0, 0, 0, 0, 1}) == kReserved);
  CHECK(v6Refusal({0x0100, 0, 0, 0, 0, 0, 0, 1}) == kReserved);
  CHECK(v6Refusal({0x2001, 0x0db8, 0, 0, 0, 0, 0, 1}) == kReserved);
}

TEST_CASE(
    "NetPolicy judges an IPv6 address carrying an IPv4 one by that address",
    "[scriptnet]")
{
  // Mapped: ::ffff:a.b.c.d.
  CHECK(v6Refusal({0, 0, 0, 0, 0, 0xffff, 0x7f00, 0x0001}) == kLoopback);
  CHECK(v6Refusal({0, 0, 0, 0, 0, 0xffff, 0x0a00, 0x0001}) == kPrivate);
  CHECK(v6Refusal({0, 0, 0, 0, 0, 0xffff, 0x0101, 0x0101}) == "");
  // The deprecated compatible form: ::a.b.c.d.
  CHECK(v6Refusal({0, 0, 0, 0, 0, 0, 0xc0a8, 0x0001}) == kPrivate);
  CHECK(v6Refusal({0, 0, 0, 0, 0, 0, 0x0101, 0x0101}) == "");
  // NAT64: 64:ff9b::a.b.c.d, and the local-use 64:ff9b:1::/48.
  CHECK(v6Refusal({0x0064, 0xff9b, 0, 0, 0, 0, 0x0a00, 0x0001}) == kPrivate);
  CHECK(v6Refusal({0x0064, 0xff9b, 0, 0, 0, 0, 0x0101, 0x0101}) == "");
  CHECK(v6Refusal({0x0064, 0xff9b, 1, 0, 0, 0, 0x0101, 0x0101}) == kPrivate);
  // 6to4: 2002:abcd::/16 with a.b.c.d right after the prefix.
  CHECK(v6Refusal({0x2002, 0x0a00, 0x0001, 0, 0, 0, 0, 1}) == kPrivate);
  CHECK(v6Refusal({0x2002, 0xa9fe, 0x0001, 0, 0, 0, 0, 1}) == kLinkLocal);
  CHECK(v6Refusal({0x2002, 0x0101, 0x0101, 0, 0, 0, 0, 1}) == "");
}

TEST_CASE("NetPolicy refuses an address of another size", "[scriptnet]")
{
  const char* unknown = "the host resolves to an address of an unknown kind";
  CHECK(addressRefusal({1, 2, 3}) == unknown);
  CHECK(addressRefusal({1, 2, 3, 4, 5}) == unknown);
}

TEST_CASE("NetPolicy refuses Rive's own hosts", "[scriptnet]")
{
  CHECK(urlRefusal("https://rive.app/") == kRiveHost);
  CHECK(urlRefusal("https://API.Rive.App/api/me") == kRiveHost);
  CHECK(urlRefusal("https://editor.rive.app./file") == kRiveHost);
  CHECK(urlRefusal("https://2dimensions.com/") == kRiveHost);
  CHECK(urlRefusal("https://net.riveusercontent.com/v1/") == kRiveHost);
}

TEST_CASE("NetPolicy upper-cases and limits methods", "[scriptnet]")
{
  HttpRequest request;
  request.url = "https://example.com/";
  request.method = "patch";
  CHECK(refusal(request).empty());
  CHECK(request.method == "PATCH");

  request.method = "";
  CHECK(refusal(request).empty());
  CHECK(request.method == "GET");

  request.method = "CONNECT";
  CHECK(refusal(request) == "the CONNECT method is not allowed");
  request.method = "trace";
  CHECK(refusal(request) == "the TRACE method is not allowed");
  request.method = "G ET";
  CHECK(refusal(request) == "the method is not a valid HTTP token");

  request.method = "GET";
  request.body = {1, 2, 3};
  CHECK(refusal(request) == "GET and HEAD requests cannot have a body");
  request.method = "HEAD";
  CHECK(refusal(request) == "GET and HEAD requests cannot have a body");
  request.method = "POST";
  CHECK(refusal(request).empty());
}

TEST_CASE("NetPolicy keeps scripts off forbidden headers", "[scriptnet]")
{
  auto headerRefusal = [](const std::string& name, const std::string& value) {
    HttpRequest request;
    request.url = "https://example.com/";
    request.headers.push_back({name, value});
    return refusal(request);
  };
  CHECK(headerRefusal("Authorization", "Bearer token").empty());
  CHECK(headerRefusal("Content-Type", "application/json").empty());
  CHECK(headerRefusal("X-Custom", "a\tb").empty());

  CHECK(
      headerRefusal("Cookie", "a=b") ==
      "scripts cannot set the 'Cookie' header");
  CHECK(
      headerRefusal("cookie2", "a=b") ==
      "scripts cannot set the 'cookie2' header");
  CHECK(
      headerRefusal("Host", "rive.app") ==
      "scripts cannot set the 'Host' header");
  CHECK(
      headerRefusal("Origin", "https://editor.rive.app") ==
      "scripts cannot set the 'Origin' header");
  CHECK(
      headerRefusal("Referer", "x") ==
      "scripts cannot set the 'Referer' header");
  CHECK(
      headerRefusal("Content-Length", "1") ==
      "scripts cannot set the 'Content-Length' header");
  CHECK(
      headerRefusal("Sec-Fetch-Site", "same-origin") ==
      "scripts cannot set the 'Sec-Fetch-Site' header");
  CHECK(
      headerRefusal("Proxy-Authorization", "x") ==
      "scripts cannot set the 'Proxy-Authorization' header");
  CHECK(
      headerRefusal("X-HTTP-Method-Override", "DELETE") ==
      "scripts cannot set the 'X-HTTP-Method-Override' header");

  CHECK(
      headerRefusal("Bad Name", "x") ==
      "a header name is not a valid HTTP token");
  CHECK(headerRefusal("", "x") == "a header name is not a valid HTTP token");
  CHECK(
      headerRefusal("X-Split", "a\r\nInjected: 1") ==
      "the value of the 'X-Split' header must be printable ASCII");
  CHECK(
      headerRefusal("X-Accent", "caf\xC3\xA9") ==
      "the value of the 'X-Accent' header must be printable ASCII");

  CHECK(NetPolicy::isForbiddenHeader("SEC-WEBSOCKET-KEY"));
  CHECK_FALSE(NetPolicy::isForbiddenHeader("Accept"));
}

TEST_CASE("NetPolicy enforces size limits", "[scriptnet]")
{
  NetLimits limits;
  limits.maxUrlLength = 32;
  limits.maxRequestBodyBytes = 4;
  limits.maxHeaderCount = 2;
  limits.maxHeaderBytes = 16;

  HttpRequest request;
  request.url = "https://example.com/a-path-that-is-too-long";
  CHECK(refusal(request, limits) == "the URL is longer than 32 characters");

  request.url = "https://example.com/";
  request.method = "POST";
  request.body = {1, 2, 3, 4, 5};
  CHECK(refusal(request, limits) == "the request body is larger than 4 bytes");
  request.body = {1, 2, 3, 4};
  CHECK(refusal(request, limits).empty());

  request.headers = {{"A", "1"}, {"B", "2"}, {"C", "3"}};
  CHECK(refusal(request, limits) == "more than 2 headers");
  request.headers = {{"X-Long", "0123456789abcdef"}};
  CHECK(refusal(request, limits) == "the headers are larger than 16 bytes");
}

TEST_CASE("NetPolicy defaults and clamps the timeout", "[scriptnet]")
{
  NetLimits limits;
  HttpRequest request;
  request.url = "https://example.com/";
  CHECK(refusal(request, limits).empty());
  CHECK(request.timeoutMs == limits.defaultTimeoutMs);

  request.timeoutMs = 5000;
  CHECK(refusal(request, limits).empty());
  CHECK(request.timeoutMs == 5000);

  request.timeoutMs = limits.maxTimeoutMs * 10;
  CHECK(refusal(request, limits).empty());
  CHECK(request.timeoutMs == limits.maxTimeoutMs);
}

namespace
{
// A request as a transport last sent it: through check(), with `headers`.
HttpRequest sentRequest(
    const std::string& url,
    const std::string& method = "GET",
    HttpHeaders headers = {},
    std::vector<uint8_t> body = {})
{
  HttpRequest request;
  request.url = url;
  request.method = method;
  request.headers = std::move(headers);
  request.body = std::move(body);
  REQUIRE(refusal(request).empty());
  return request;
}

// The URL a redirect lands on, or the refusal.
std::string hopUrl(
    const std::string& from,
    uint16_t status,
    const std::string& location)
{
  HttpRequest request = sentRequest(from);
  NetError error;
  if (!NetPolicy::redirect(request, status, location, 0, NetLimits(), &error))
  {
    return "refused: " + error.message;
  }
  return request.url;
}

bool hasHeader(const HttpRequest& request, const char* name)
{
  for (const HttpHeader& header : request.headers)
  {
    if (lowerAscii(header.name) == name)
    {
      return true;
    }
  }
  return false;
}
} // namespace

TEST_CASE(
    "NetPolicy::redirect resolves the Location against the request's URL",
    "[scriptnet]")
{
  const std::string from = "https://example.com/a/b?y=1";
  CHECK(
      hopUrl(from, 302, "https://other.example.com/x") ==
      "https://other.example.com/x");
  CHECK(
      hopUrl(from, 302, "//cdn.example.com/a") == "https://cdn.example.com/a");
  CHECK(hopUrl(from, 302, "/root?q=1") == "https://example.com/root?q=1");
  CHECK(hopUrl(from, 302, "c") == "https://example.com/a/c");
  CHECK(hopUrl(from, 302, "?x=2") == "https://example.com/a/b?x=2");
  CHECK(hopUrl(from, 302, "#section") == "https://example.com/a/b?y=1");
  // Raw UTF-8, spaces and backslashes are percent-encoded, as browsers do;
  // what is already encoded stays.
  CHECK(
      hopUrl(from, 302, "/caf\xC3\xA9 menu\\%20x") ==
      "https://example.com/caf%C3%A9%20menu%5C%20x");

  CHECK(hopUrl(from, 302, "  /spaced  ") == "https://example.com/spaced");
  CHECK(hopUrl(from, 302, "/p#frag") == "https://example.com/p");
  CHECK(
      hopUrl("https://example.com/a/b/", 302, "../d") ==
      "https://example.com/a/d");
  CHECK(
      hopUrl("https://example.com/a/b/", 302, "./") ==
      "https://example.com/a/b/");
  CHECK(hopUrl("https://example.com", 302, "x") == "https://example.com/x");
  CHECK(
      hopUrl(from, 302, "") ==
      "refused: the Location header could not be resolved");
}

TEST_CASE(
    "NetPolicy::redirect continues as a GET where browsers do",
    "[scriptnet]")
{
  HttpHeaders headers = {
      {"Content-Type", "text/plain"},
      {"X-Keep", "1"},
  };
  std::vector<uint8_t> body = {'p', 'i', 'n', 'g'};
  NetError error;

  HttpRequest seeOther = sentRequest(
      "https://example.com/",
      "PUT",
      headers,
      body);
  REQUIRE(NetPolicy::redirect(seeOther, 303, "/next", 0, NetLimits(), &error));
  CHECK(seeOther.method == "GET");
  CHECK(seeOther.body.empty());
  CHECK_FALSE(hasHeader(seeOther, "content-type"));
  CHECK(hasHeader(seeOther, "x-keep"));

  HttpRequest found = sentRequest(
      "https://example.com/",
      "POST",
      headers,
      body);
  REQUIRE(NetPolicy::redirect(found, 302, "/next", 0, NetLimits(), &error));
  CHECK(found.method == "GET");
  CHECK(found.body.empty());

  HttpRequest put = sentRequest("https://example.com/", "PUT", headers, body);
  REQUIRE(NetPolicy::redirect(put, 302, "/next", 0, NetLimits(), &error));
  CHECK(put.method == "PUT");
  CHECK(put.body == body);
  CHECK(hasHeader(put, "content-type"));

  HttpRequest temporary = sentRequest(
      "https://example.com/",
      "POST",
      headers,
      body);
  REQUIRE(NetPolicy::redirect(temporary, 307, "/next", 0, NetLimits(), &error));
  CHECK(temporary.method == "POST");
  CHECK(temporary.body == body);
}

TEST_CASE(
    "NetPolicy::redirect drops Authorization once the hop leaves the origin",
    "[scriptnet]")
{
  HttpHeaders headers = {
      {"Authorization", "Bearer secret"},
      {"X-Keep", "1"},
  };
  NetError error;

  HttpRequest sameOrigin = sentRequest("https://example.com/a", "GET", headers);
  REQUIRE(NetPolicy::redirect(sameOrigin, 302, "/b", 0, NetLimits(), &error));
  CHECK(hasHeader(sameOrigin, "authorization"));

  HttpRequest defaultPort = sentRequest(
      "https://example.com:443/a",
      "GET",
      headers);
  REQUIRE(NetPolicy::redirect(
      defaultPort,
      302,
      "https://example.com/b",
      0,
      NetLimits(),
      &error));
  CHECK(hasHeader(defaultPort, "authorization"));

  // The port counts by value, as check() keeps it as written.
  HttpRequest zeroPadded = sentRequest(
      "https://example.com:0443/a",
      "GET",
      headers);
  REQUIRE(NetPolicy::redirect(
      zeroPadded,
      302,
      "https://example.com/b",
      0,
      NetLimits(),
      &error));
  CHECK(hasHeader(zeroPadded, "authorization"));

  HttpRequest samePort = sentRequest(
      "https://example.com:08443/a",
      "GET",
      headers);
  REQUIRE(NetPolicy::redirect(
      samePort,
      302,
      "https://example.com:8443/b",
      0,
      NetLimits(),
      &error));
  CHECK(hasHeader(samePort, "authorization"));

  HttpRequest otherHost = sentRequest("https://example.com/a", "GET", headers);
  REQUIRE(NetPolicy::redirect(
      otherHost,
      302,
      "https://other.example.com/b",
      0,
      NetLimits(),
      &error));
  CHECK_FALSE(hasHeader(otherHost, "authorization"));
  CHECK(hasHeader(otherHost, "x-keep"));

  HttpRequest otherPort = sentRequest("https://example.com/a", "GET", headers);
  REQUIRE(NetPolicy::redirect(
      otherPort,
      302,
      "https://example.com:8443/b",
      0,
      NetLimits(),
      &error));
  CHECK_FALSE(hasHeader(otherPort, "authorization"));
}

TEST_CASE(
    "NetPolicy::redirect refuses hops the policy would, and past the limit",
    "[scriptnet]")
{
  const std::string from = "https://example.com/";
  CHECK(
      hopUrl(from, 302, "http://example.com/") ==
      "refused: http: URLs are not allowed; use https:");
  CHECK(
      hopUrl(from, 302, "https://api.rive.app/") ==
      std::string("refused: ") + kRiveHost);
  CHECK(
      hopUrl(from, 302, "https://10.0.0.1/") ==
      std::string("refused: ") + kIpHost);

  HttpRequest request = sentRequest(from);
  NetLimits limits;
  NetError error;
  CHECK(NetPolicy::redirect(
      request,
      302,
      "/a",
      limits.maxRedirects - 1,
      limits,
      &error));
  CHECK_FALSE(NetPolicy::redirect(
      request,
      302,
      "/b",
      limits.maxRedirects,
      limits,
      &error));
  CHECK(error.code == NetErrorCode::Network);
  CHECK(error.message == "more than 20 redirects");
  // Refused hops leave the request as it was.
  CHECK(request.url == "https://example.com/a");
}

#endif // WITH_RIVE_SCRIPTNET
