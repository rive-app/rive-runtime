#if defined(WITH_RIVE_SCRIPTNET) && (defined(__APPLE__) || defined(_WIN32))

#include "catch.hpp"
#include "rive/scriptnet/net.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Drives the real platform provider (NSURLSession on Apple, WinHTTP on
// Windows) against a server on 127.0.0.1. The policy refuses http: and IP
// hosts, so requests go through fetch() with a policy-valid https URL and a
// wrapper provider rewrites them to the loopback server before handing them
// to the platform provider.

using namespace rive;

namespace
{
// The few places sockets differ between BSD and Winsock.
#ifdef _WIN32
using Socket = SOCKET;
using SockLen = int;
constexpr Socket kInvalidSocket = INVALID_SOCKET;

void closeSocket(Socket socket) { closesocket(socket); }

bool readable(Socket socket, int timeoutMs)
{
  WSAPOLLFD fd = {socket, POLLRDNORM, 0};
  return WSAPoll(&fd, 1, timeoutMs) > 0;
}

void setReceiveTimeout(Socket socket, int seconds)
{
  DWORD timeout = (DWORD)seconds * 1000;
  setsockopt(
      socket,
      SOL_SOCKET,
      SO_RCVTIMEO,
      reinterpret_cast<const char*>(&timeout),
      sizeof(timeout));
}

void startSockets()
{
  static bool started = [] {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  (void)started;
}
#else
using Socket = int;
using SockLen = socklen_t;
constexpr Socket kInvalidSocket = -1;

void closeSocket(Socket socket) { close(socket); }

bool readable(Socket socket, int timeoutMs)
{
  pollfd fd = {socket, POLLIN, 0};
  return ::poll(&fd, 1, timeoutMs) > 0;
}

void setReceiveTimeout(Socket socket, int seconds)
{
  timeval timeout = {seconds, 0};
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

void startSockets() {}
#endif

struct CannedResponse
{
  std::string text;
  int delayMs = 0;
};

// Serves one canned response per connection, in order, and records each raw
// request.
class LoopbackServer
{
public:
  explicit LoopbackServer(std::vector<CannedResponse> responses)
      : m_responses(std::move(responses))
  {
    startSockets();
    m_listener = socket(AF_INET, SOCK_STREAM, 0);
#ifndef _WIN32
    int yes = 1;
    setsockopt(m_listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    bind(m_listener, (sockaddr*)&address, sizeof(address));
    listen(m_listener, 4);
    SockLen length = sizeof(address);
    getsockname(m_listener, (sockaddr*)&address, &length);
    m_port = ntohs(address.sin_port);
    m_thread = std::thread([this] { serve(); });
  }

  ~LoopbackServer()
  {
    m_stopping = true;
    m_thread.join();
    closeSocket(m_listener);
  }

  uint16_t port() const { return m_port; }

  std::vector<std::string> requests()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_requests;
  }

private:
  void serve()
  {
    for (const CannedResponse& canned : m_responses)
    {
      Socket connection = acceptWithTimeout();
      if (connection == kInvalidSocket)
      {
        return;
      }
#ifdef SO_NOSIGPIPE
      int yes = 1;
      // The client may already have given up; don't die of SIGPIPE.
      setsockopt(connection, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif
      std::string request = readRequest(connection);
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_requests.push_back(request);
      }
      for (int waited = 0; waited < canned.delayMs && !m_stopping; waited += 10)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      send(connection, canned.text.data(), (int)canned.text.size(), 0);
      closeSocket(connection);
    }
  }

  Socket acceptWithTimeout()
  {
    for (int waited = 0; waited < 5000 && !m_stopping; waited += 50)
    {
      if (readable(m_listener, 50))
      {
        return accept(m_listener, nullptr, nullptr);
      }
    }
    return kInvalidSocket;
  }

  static std::string readRequest(Socket connection)
  {
    setReceiveTimeout(connection, 2);
    std::string request;
    char chunk[4096];
    size_t headerEnd = std::string::npos;
    size_t contentLength = 0;
    while (true)
    {
      if (headerEnd != std::string::npos &&
          request.size() >= headerEnd + 4 + contentLength)
      {
        break;
      }
      auto received = recv(connection, chunk, sizeof(chunk), 0);
      if (received <= 0)
      {
        break;
      }
      request.append(chunk, (size_t)received);
      if (headerEnd == std::string::npos)
      {
        headerEnd = request.find("\r\n\r\n");
        if (headerEnd != std::string::npos)
        {
          std::string lower = request.substr(0, headerEnd);
          std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
          size_t field = lower.find("content-length:");
          if (field != std::string::npos)
          {
            contentLength = (size_t)
                std::strtoul(lower.c_str() + field + 15, nullptr, 10);
          }
        }
      }
    }
    return request;
  }

  std::vector<CannedResponse> m_responses;
  Socket m_listener = kInvalidSocket;
  uint16_t m_port = 0;
  std::atomic<bool> m_stopping{false};
  std::mutex m_mutex;
  std::vector<std::string> m_requests;
  std::thread m_thread;
};

// Rewrites https://<any host>/<path> to the loopback server and forwards to
// the real platform provider.
class LoopbackProvider : public scriptnet::Provider
{
public:
  explicit LoopbackProvider(uint16_t port)
      : m_inner(scriptnet::makePlatformProvider())
      , m_port(port)
  {}

  void start(scriptnet::RequestId id, const scriptnet::HttpRequest& request)
      override
  {
    scriptnet::HttpRequest local = request;
    size_t path = local.url.find('/', std::strlen("https://"));
    local.url = "http://127.0.0.1:" + std::to_string(m_port) +
                (path == std::string::npos ? "/" : local.url.substr(path));
    m_inner->start(id, local);
  }

  void abort(scriptnet::RequestId id) override { m_inner->abort(id); }

private:
  rcp<scriptnet::Provider> m_inner;
  uint16_t m_port;
};

class RecordingListener : public scriptnet::FetchListener
{
public:
  bool done = false;
  bool ok = false;
  scriptnet::HttpResponse response;
  scriptnet::NetError error;

  void onResponse(scriptnet::HttpResponse&& value) override
  {
    done = true;
    ok = true;
    response = std::move(value);
  }
  void onError(const scriptnet::NetError& value) override
  {
    done = true;
    error = value;
  }
};

// Installs the loopback transport for one test and restores the default.
struct LoopbackNetwork
{
  explicit LoopbackNetwork(uint16_t port)
  {
    scriptnet::setProvider(make_rcp<LoopbackProvider>(port));
    // The server is on loopback, which a transport that checks addresses
    // would refuse.
    scriptnet::NetLimits limits;
    limits.allowLocalAddresses = true;
    scriptnet::setLimits(limits);
  }
  ~LoopbackNetwork()
  {
    scriptnet::cancelAllForOwner(kOwner);
    scriptnet::setProvider(nullptr);
    scriptnet::setLimits(scriptnet::NetLimits());
  }
  static constexpr uint64_t kOwner = 0xA11E0001;
};

// Starts a fetch and polls until it is delivered, or 30 s pass: a
// sanitizer build deep into a long run has needed more than 5 s.
rcp<RecordingListener> fetchAndWait(scriptnet::HttpRequest request)
{
  auto listener = make_rcp<RecordingListener>();
  scriptnet::fetch(LoopbackNetwork::kOwner, std::move(request), listener);
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!listener->done && std::chrono::steady_clock::now() < deadline)
  {
    scriptnet::poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return listener;
}

scriptnet::HttpRequest get(const std::string& path)
{
  scriptnet::HttpRequest request;
  request.url = "https://fixture.example.com" + path;
  return request;
}

std::string lowered(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), ::tolower);
  return value;
}
} // namespace

TEST_CASE(
    "the platform provider round-trips a request and its response",
    "[scriptnet][platform]")
{
  LoopbackServer server({{"HTTP/1.1 201 Created\r\n"
                          "Content-Type: text/plain\r\n"
                          "X-Test: yes\r\n"
                          "Content-Length: 5\r\n"
                          "Connection: close\r\n\r\n"
                          "hello"}});
  LoopbackNetwork network(server.port());

  scriptnet::HttpRequest request = get("/items?page=2");
  request.method = "POST";
  request.headers = {{"X-Script", "1"}, {"Content-Type", "text/plain"}};
  request.body = {'p', 'i', 'n', 'g'};
  auto listener = fetchAndWait(std::move(request));

  REQUIRE(listener->done);
  REQUIRE(listener->ok);
  CHECK(listener->response.status == 201);
  CHECK(listener->response.statusText == "Created");
  CHECK(
      std::string(
          listener->response.body.begin(),
          listener->response.body.end()) == "hello");
  bool sawHeader = false;
  for (const scriptnet::HttpHeader& header : listener->response.headers)
  {
    sawHeader = sawHeader ||
                (lowered(header.name) == "x-test" && header.value == "yes");
  }
  CHECK(sawHeader);

  std::vector<std::string> requests = server.requests();
  REQUIRE(requests.size() == 1);
  CHECK(requests[0].rfind("POST /items?page=2 HTTP/1.1\r\n", 0) == 0);
  CHECK(lowered(requests[0]).find("x-script: 1\r\n") != std::string::npos);
  CHECK(requests[0].substr(requests[0].size() - 4) == "ping");
}

TEST_CASE(
    "the platform provider never stores or sends cookies",
    "[scriptnet][platform]")
{
  LoopbackServer server(
      {{"HTTP/1.1 200 OK\r\n"
        "Set-Cookie: session=secret; Path=/\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n"},
       {"HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n"}});
  LoopbackNetwork network(server.port());

  REQUIRE(fetchAndWait(get("/login"))->ok);
  REQUIRE(fetchAndWait(get("/me"))->ok);

  std::vector<std::string> requests = server.requests();
  REQUIRE(requests.size() == 2);
  CHECK(lowered(requests[1]).find("cookie:") == std::string::npos);
  CHECK(requests[1].find("secret") == std::string::npos);
}

TEST_CASE(
    "the platform provider re-checks each redirect against the policy",
    "[scriptnet][platform]")
{
  LoopbackServer server({{"HTTP/1.1 302 Found\r\n"
                          "Location: https://api.rive.app/api/me\r\n"
                          "Content-Length: 0\r\n"
                          "Connection: close\r\n\r\n"}});
  LoopbackNetwork network(server.port());

  auto listener = fetchAndWait(get("/redirect"));
  REQUIRE(listener->done);
  CHECK_FALSE(listener->ok);
  CHECK(listener->error.code == scriptnet::NetErrorCode::Policy);
  CHECK(
      listener->error.message ==
      "a redirect was refused: Rive's own hosts are not reachable from "
      "scripts");
}

TEST_CASE(
    "the platform provider refuses a redirect to http:",
    "[scriptnet][platform]")
{
  // WinHTTP would keep an https: to http: hop to itself by default; the
  // policy must see it, as on Apple, so the script gets the same refusal.
  LoopbackServer server({{"HTTP/1.1 302 Found\r\n"
                          "Location: http://example.com/\r\n"
                          "Content-Length: 0\r\n"
                          "Connection: close\r\n\r\n"}});
  LoopbackNetwork network(server.port());

  auto listener = fetchAndWait(get("/downgrade"));
  REQUIRE(listener->done);
  CHECK_FALSE(listener->ok);
  CHECK(listener->error.code == scriptnet::NetErrorCode::Policy);
  CHECK(
      listener->error.message ==
      "a redirect was refused: http: URLs are not allowed; use https:");
}

TEST_CASE(
    "the platform provider stops a body over the size limit",
    "[scriptnet][platform]")
{
  LoopbackServer server(
      {{"HTTP/1.1 200 OK\r\n"
        "Content-Length: 100\r\n"
        "Connection: close\r\n\r\n" +
        std::string(100, 'x')}});
  LoopbackNetwork network(server.port());
  scriptnet::NetLimits limits = scriptnet::limits();
  limits.maxResponseBodyBytes = 8;
  scriptnet::setLimits(limits);

  auto listener = fetchAndWait(get("/large"));
  REQUIRE(listener->done);
  CHECK_FALSE(listener->ok);
  CHECK(listener->error.code == scriptnet::NetErrorCode::TooLarge);
}

TEST_CASE(
    "the platform provider answers a HEAD for a resource over the size limit",
    "[scriptnet][platform]")
{
  // The Content-Length of a HEAD answer describes the resource, which never
  // follows.
  LoopbackServer server({{"HTTP/1.1 200 OK\r\n"
                          "Content-Length: 100\r\n"
                          "Connection: close\r\n\r\n"}});
  LoopbackNetwork network(server.port());
  scriptnet::NetLimits limits = scriptnet::limits();
  limits.maxResponseBodyBytes = 8;
  scriptnet::setLimits(limits);

  scriptnet::HttpRequest request = get("/large");
  request.method = "HEAD";
  auto listener = fetchAndWait(std::move(request));
  REQUIRE(listener->done);
  REQUIRE(listener->ok);
  CHECK(listener->response.status == 200);
  CHECK(listener->response.body.empty());
  bool sawLength = false;
  for (const scriptnet::HttpHeader& header : listener->response.headers)
  {
    sawLength = sawLength || (lowered(header.name) == "content-length" &&
                              header.value == "100");
  }
  CHECK(sawLength);
}

TEST_CASE(
    "the platform provider answers a 304 for a resource over the size limit",
    "[scriptnet][platform]")
{
  // A 304 may name the Content-Length of the representation it stands for.
  LoopbackServer server({{"HTTP/1.1 304 Not Modified\r\n"
                          "Content-Length: 100\r\n"
                          "Connection: close\r\n\r\n"}});
  LoopbackNetwork network(server.port());
  scriptnet::NetLimits limits = scriptnet::limits();
  limits.maxResponseBodyBytes = 8;
  scriptnet::setLimits(limits);

  scriptnet::HttpRequest request = get("/cached");
  request.headers = {{"If-None-Match", "\"v1\""}};
  auto listener = fetchAndWait(std::move(request));
  REQUIRE(listener->done);
  REQUIRE(listener->ok);
  CHECK(listener->response.status == 304);
  CHECK(listener->response.body.empty());
}

TEST_CASE(
    "the platform provider enforces the script's total timeout",
    "[scriptnet][platform]")
{
  CannedResponse slow;
  slow.text = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n"
              "Connection: close\r\n\r\n";
  slow.delayMs = 2000;
  LoopbackServer server({slow});
  LoopbackNetwork network(server.port());

  scriptnet::HttpRequest request = get("/slow");
  request.timeoutMs = 200;
  auto started = std::chrono::steady_clock::now();
  auto listener = fetchAndWait(std::move(request));
  auto elapsed = std::chrono::steady_clock::now() - started;

  REQUIRE(listener->done);
  CHECK_FALSE(listener->ok);
  CHECK(listener->error.code == scriptnet::NetErrorCode::Timeout);
  CHECK(elapsed < std::chrono::milliseconds(1500));
}

TEST_CASE(
    "the platform provider settles a timeout shorter than its own setup",
    "[scriptnet][platform]")
{
  // The deadline can pass while the transport is still opening the
  // request; the answer is still a timeout, never a crash or a hang. The
  // server accepts no connection, so nothing could answer either way.
  LoopbackServer server({});
  LoopbackNetwork network(server.port());

  scriptnet::HttpRequest request = get("/never");
  request.timeoutMs = 1;
  auto listener = fetchAndWait(std::move(request));
  REQUIRE(listener->done);
  CHECK_FALSE(listener->ok);
  CHECK(listener->error.code == scriptnet::NetErrorCode::Timeout);
}

TEST_CASE(
    "the platform provider delivers a 3xx without a Location as an answer",
    "[scriptnet][platform]")
{
  LoopbackServer server({{"HTTP/1.1 304 Not Modified\r\n"
                          "ETag: \"v2\"\r\n"
                          "Content-Length: 0\r\n"
                          "Connection: close\r\n\r\n"}});
  LoopbackNetwork network(server.port());

  auto listener = fetchAndWait(get("/cached"));
  REQUIRE(listener->done);
  REQUIRE(listener->ok);
  CHECK(listener->response.status == 304);
}

#ifdef _WIN32
// WinHTTP decodes header bytes with the system's ANSI code page; the
// transport turns them back into the bytes the server sent.
TEST_CASE(
    "the Windows provider hands header bytes through untouched",
    "[scriptnet][platform]")
{
  LoopbackServer server({{std::string("HTTP/1.1 200 OK\r\n"
                                      "X-Bytes: caf\xC3\xA9 \x80\r\n"
                                      "Content-Length: 0\r\n"
                                      "Connection: close\r\n\r\n")}});
  LoopbackNetwork network(server.port());

  auto listener = fetchAndWait(get("/bytes"));
  REQUIRE(listener->ok);
  std::string value;
  for (const scriptnet::HttpHeader& header : listener->response.headers)
  {
    if (lowered(header.name) == "x-bytes")
    {
      value = header.value;
    }
  }
  CHECK(value == std::string("caf\xC3\xA9 \x80"));
}

TEST_CASE(
    "the Windows provider percent-encodes a raw UTF-8 Location before the "
    "policy sees it",
    "[scriptnet][platform]")
{
  // Refused for its host, not for the bytes: the encoding came first.
  LoopbackServer server(
      {{std::string("HTTP/1.1 302 Found\r\n"
                    "Location: https://api.rive.app/caf\xC3\xA9 x\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n\r\n")}});
  LoopbackNetwork network(server.port());

  auto listener = fetchAndWait(get("/redirect"));
  REQUIRE(listener->done);
  CHECK_FALSE(listener->ok);
  CHECK(listener->error.code == scriptnet::NetErrorCode::Policy);
  CHECK(
      listener->error.message ==
      "a redirect was refused: Rive's own hosts are not reachable from "
      "scripts");
}
#endif

#ifdef _WIN32
TEST_CASE(
    "the Windows provider refuses a host that resolves to a local address "
    "before sending",
    "[scriptnet][platform]")
{
  LoopbackServer server({{"HTTP/1.1 200 OK\r\n"
                          "Content-Length: 0\r\n"
                          "Connection: close\r\n\r\n"}});
  LoopbackNetwork network(server.port());
  // As outside tests: the loopback server stands in for a public name
  // whose address turns out to be local.
  scriptnet::setLimits(scriptnet::NetLimits());

  auto listener = fetchAndWait(get("/intranet"));
  REQUIRE(listener->done);
  CHECK_FALSE(listener->ok);
  CHECK(listener->error.code == scriptnet::NetErrorCode::Policy);
  CHECK(listener->error.message == "the host resolves to a loopback address");
  // No request reached the server. (WinHTTP does connect before the close
  // takes effect, so a connection with nothing on it may be recorded as an
  // empty request after the server's receive timeout, well after this.)
  for (const std::string& request : server.requests())
  {
    CHECK(request.empty());
  }
}
#endif

#endif // WITH_RIVE_SCRIPTNET && (__APPLE__ || _WIN32)
