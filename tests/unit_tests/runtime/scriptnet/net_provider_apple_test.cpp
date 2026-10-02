#if defined(WITH_RIVE_SCRIPTNET) && defined(__APPLE__)

#include "catch.hpp"
#include "rive/scriptnet/net.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Drives the real NSURLSession provider against a server on 127.0.0.1. The
// policy refuses http: and IP hosts, so requests go through fetch() with a
// policy-valid https URL and a wrapper provider rewrites them to the loopback
// server before handing them to the platform provider.

using namespace rive;

namespace
{
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
    explicit LoopbackServer(std::vector<CannedResponse> responses) :
        m_responses(std::move(responses))
    {
        m_listener = socket(AF_INET, SOCK_STREAM, 0);
        int yes = 1;
        setsockopt(m_listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        bind(m_listener, (sockaddr*)&address, sizeof(address));
        listen(m_listener, 4);
        socklen_t length = sizeof(address);
        getsockname(m_listener, (sockaddr*)&address, &length);
        m_port = ntohs(address.sin_port);
        m_thread = std::thread([this] { serve(); });
    }

    ~LoopbackServer()
    {
        m_stopping = true;
        m_thread.join();
        close(m_listener);
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
            int connection = acceptWithTimeout();
            if (connection < 0)
            {
                return;
            }
            int yes = 1;
            // The client may already have given up; don't die of SIGPIPE.
            setsockopt(connection, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
            std::string request = readRequest(connection);
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_requests.push_back(request);
            }
            for (int waited = 0; waited < canned.delayMs && !m_stopping;
                 waited += 10)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            send(connection, canned.text.data(), canned.text.size(), 0);
            close(connection);
        }
    }

    int acceptWithTimeout()
    {
        for (int waited = 0; waited < 5000 && !m_stopping; waited += 50)
        {
            pollfd listener = {m_listener, POLLIN, 0};
            if (::poll(&listener, 1, 50) > 0)
            {
                return accept(m_listener, nullptr, nullptr);
            }
        }
        return -1;
    }

    static std::string readRequest(int connection)
    {
        timeval timeout = {2, 0};
        setsockopt(connection,
                   SOL_SOCKET,
                   SO_RCVTIMEO,
                   &timeout,
                   sizeof(timeout));
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
            ssize_t received = recv(connection, chunk, sizeof(chunk), 0);
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
                    std::transform(lower.begin(),
                                   lower.end(),
                                   lower.begin(),
                                   ::tolower);
                    size_t field = lower.find("content-length:");
                    if (field != std::string::npos)
                    {
                        contentLength =
                            (size_t)std::strtoul(lower.c_str() + field + 15,
                                                 nullptr,
                                                 10);
                    }
                }
            }
        }
        return request;
    }

    std::vector<CannedResponse> m_responses;
    int m_listener = -1;
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
    explicit LoopbackProvider(uint16_t port) :
        m_inner(scriptnet::makePlatformProvider()), m_port(port)
    {}

    void start(scriptnet::RequestId id,
               const scriptnet::HttpRequest& request) override
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
        scriptnet::setLimits(scriptnet::NetLimits());
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

TEST_CASE("the Apple provider round-trips a request and its response",
          "[scriptnet][apple]")
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
    CHECK(std::string(listener->response.body.begin(),
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

TEST_CASE("the Apple provider never stores or sends cookies",
          "[scriptnet][apple]")
{
    LoopbackServer server({{"HTTP/1.1 200 OK\r\n"
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

TEST_CASE("the Apple provider re-checks each redirect against the policy",
          "[scriptnet][apple]")
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
    CHECK(listener->error.message ==
          "a redirect was refused: Rive's own hosts are not reachable from "
          "scripts");
}

TEST_CASE("the Apple provider stops a body over the size limit",
          "[scriptnet][apple]")
{
    LoopbackServer server({{"HTTP/1.1 200 OK\r\n"
                            "Content-Length: 100\r\n"
                            "Connection: close\r\n\r\n" +
                            std::string(100, 'x')}});
    LoopbackNetwork network(server.port());
    scriptnet::NetLimits limits;
    limits.maxResponseBodyBytes = 8;
    scriptnet::setLimits(limits);

    auto listener = fetchAndWait(get("/large"));
    REQUIRE(listener->done);
    CHECK_FALSE(listener->ok);
    CHECK(listener->error.code == scriptnet::NetErrorCode::TooLarge);
}

TEST_CASE("the Apple provider enforces the script's total timeout",
          "[scriptnet][apple]")
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

#endif // WITH_RIVE_SCRIPTNET && __APPLE__
