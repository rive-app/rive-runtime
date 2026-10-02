#ifdef WITH_RIVE_SCRIPTNET

#include "catch.hpp"
#include "rive/scriptnet/net_policy.hpp"

#include <string>

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

TEST_CASE("NetPolicy lets https requests to public hosts through",
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
    CHECK(normalizedUrl("HTTPS://Example.COM/Path?Q=1") ==
          "https://example.com/Path?Q=1");
    CHECK(normalizedUrl("https://example.com/page#section") ==
          "https://example.com/page");
    CHECK(normalizedUrl("https://example.com.:443/") ==
          "https://example.com:443/");
    CHECK(normalizedUrl("https://example.com:/") == "https://example.com/");
    CHECK(normalizedUrl("https://example.com?x") == "https://example.com?x");
}

TEST_CASE("NetPolicy refuses every scheme but https", "[scriptnet]")
{
    CHECK(urlRefusal("http://example.com/") ==
          "http: URLs are not allowed; use https:");
    CHECK(urlRefusal("HTTP://example.com/") ==
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
    CHECK(urlRefusal("https://user:secret@example.com/") ==
          "URLs may not carry credentials");
    CHECK(urlRefusal("https://user@example.com/") ==
          "URLs may not carry credentials");
    CHECK(urlRefusal("https://example.com:0/") ==
          "the URL has an invalid port");
    CHECK(urlRefusal("https://example.com:65536/") ==
          "the URL has an invalid port");
    CHECK(urlRefusal("https://example.com:44a/") ==
          "the URL has an invalid port");
    CHECK(urlRefusal("https://example..com/") == "the host has an empty label");
    CHECK(urlRefusal("https://.example.com/") == "the host has an empty label");
    CHECK(urlRefusal("https://exa%6Dple.com/") ==
          "the host must be an ASCII domain name (punycode for international "
          "names)");
}

TEST_CASE("NetPolicy refuses IP literals in every form the URL standard reads",
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

TEST_CASE("NetPolicy refuses local network and single-label hosts",
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

    CHECK(headerRefusal("Cookie", "a=b") ==
          "scripts cannot set the 'Cookie' header");
    CHECK(headerRefusal("cookie2", "a=b") ==
          "scripts cannot set the 'cookie2' header");
    CHECK(headerRefusal("Host", "rive.app") ==
          "scripts cannot set the 'Host' header");
    CHECK(headerRefusal("Origin", "https://editor.rive.app") ==
          "scripts cannot set the 'Origin' header");
    CHECK(headerRefusal("Referer", "x") ==
          "scripts cannot set the 'Referer' header");
    CHECK(headerRefusal("Content-Length", "1") ==
          "scripts cannot set the 'Content-Length' header");
    CHECK(headerRefusal("Sec-Fetch-Site", "same-origin") ==
          "scripts cannot set the 'Sec-Fetch-Site' header");
    CHECK(headerRefusal("Proxy-Authorization", "x") ==
          "scripts cannot set the 'Proxy-Authorization' header");
    CHECK(headerRefusal("X-HTTP-Method-Override", "DELETE") ==
          "scripts cannot set the 'X-HTTP-Method-Override' header");

    CHECK(headerRefusal("Bad Name", "x") ==
          "a header name is not a valid HTTP token");
    CHECK(headerRefusal("", "x") == "a header name is not a valid HTTP token");
    CHECK(headerRefusal("X-Split", "a\r\nInjected: 1") ==
          "the value of the 'X-Split' header must be printable ASCII");
    CHECK(headerRefusal("X-Accent", "caf\xC3\xA9") ==
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
    CHECK(refusal(request, limits) ==
          "the request body is larger than 4 bytes");
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

#endif // WITH_RIVE_SCRIPTNET
