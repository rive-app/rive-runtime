#ifdef WITH_RIVE_SCRIPTNET

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/async/work_pool.hpp"
#include "rive/scriptnet/net.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace rive;

namespace
{
// Records what fetch() hands the transport; tests settle requests by hand
// with scriptnet::complete / scriptnet::fail.
class MockProvider : public scriptnet::Provider
{
public:
    struct Start
    {
        scriptnet::RequestId id;
        scriptnet::HttpRequest request;
    };
    std::vector<Start> starts;
    std::vector<scriptnet::RequestId> aborts;

    void start(scriptnet::RequestId id,
               const scriptnet::HttpRequest& request) override
    {
        starts.push_back({id, request});
    }
    void abort(scriptnet::RequestId id) override { aborts.push_back(id); }
};

// Installs a MockProvider and default limits for one test. Declared before
// the ScriptingTest so the VM closes (cancelling its requests) while the
// provider is still installed.
struct MockNetwork
{
    rcp<MockProvider> provider = make_rcp<MockProvider>();
    MockNetwork()
    {
        scriptnet::setProvider(provider);
        scriptnet::setLimits(scriptnet::NetLimits());
    }
    ~MockNetwork()
    {
        scriptnet::setProvider(nullptr);
        scriptnet::setLimits(scriptnet::NetLimits());
    }
};

scriptnet::HttpResponse textResponse(uint16_t status, const std::string& body)
{
    scriptnet::HttpResponse response;
    response.status = status;
    response.statusText = status == 200 ? "OK" : "Not Found";
    response.url = "https://example.com/";
    response.body.assign(body.begin(), body.end());
    return response;
}

// The table a test script returns, kept past the chunk by a registry ref so
// the test can read what later callbacks wrote into it.
class ScriptState
{
public:
    explicit ScriptState(lua_State* state) :
        m_state(state), m_ref(lua_ref(state, -1))
    {}
    ~ScriptState() { lua_unref(m_state, m_ref); }

    std::string string(const char* field)
    {
        push(field);
        const char* value = lua_tostring(m_state, -1);
        std::string result = value != nullptr ? value : "<nil>";
        lua_pop(m_state, 2);
        return result;
    }
    double number(const char* field)
    {
        push(field);
        double value = lua_tonumber(m_state, -1);
        lua_pop(m_state, 2);
        return value;
    }
    bool boolean(const char* field)
    {
        push(field);
        bool value = lua_toboolean(m_state, -1) != 0;
        lua_pop(m_state, 2);
        return value;
    }

private:
    void push(const char* field)
    {
        lua_getref(m_state, m_ref);
        lua_getfield(m_state, -1, field);
    }

    lua_State* m_state;
    int m_ref;
};
} // namespace

TEST_CASE("fetch returns a Promise that settles only when polled",
          "[scripting][fetch]")
{
    MockNetwork network;
    ScriptingTest test(R"(
        local state = { settled = false }
        local promise = fetch("https://example.com/data")
        promise:andThen(function(response)
            state.settled = true
            state.status = response.status
        end)
        state.before = promise:getStatus()
        return state
    )");
    ScriptState state(test.state());
    CHECK(state.string("before") == "Pending");
    REQUIRE(network.provider->starts.size() == 1);
    CHECK(network.provider->starts[0].request.url ==
          "https://example.com/data");

    CHECK(rive_hasPendingAsyncWork());
    scriptnet::complete(network.provider->starts[0].id,
                        textResponse(200, "hi"));
    CHECK_FALSE(state.boolean("settled"));
    rive_pollAsyncWork();
    CHECK(state.boolean("settled"));
    CHECK(state.number("status") == 200);
    CHECK_FALSE(rive_hasPendingAsyncWork());
}

TEST_CASE("fetch hands the provider a normalized request", "[scripting][fetch]")
{
    MockNetwork network;
    ScriptingTest test(R"(
        fetch("https://Example.com/submit#fragment", {
            method = "post",
            headers = {
                ["Content-Type"] = "application/json",
                Authorization = "Bearer token",
            },
            body = '{"a":1}',
            timeout = 5000,
        })
        return 0
    )");
    REQUIRE(network.provider->starts.size() == 1);
    const scriptnet::HttpRequest& request = network.provider->starts[0].request;
    CHECK(request.url == "https://example.com/submit");
    CHECK(request.method == "POST");
    CHECK(std::string(request.body.begin(), request.body.end()) == "{\"a\":1}");
    CHECK(request.timeoutMs == 5000);
    REQUIRE(request.headers.size() == 2);
    bool contentType = false;
    bool authorization = false;
    for (const scriptnet::HttpHeader& header : request.headers)
    {
        contentType = contentType || (header.name == "Content-Type" &&
                                      header.value == "application/json");
        authorization = authorization || (header.name == "Authorization" &&
                                          header.value == "Bearer token");
    }
    CHECK(contentType);
    CHECK(authorization);
}

TEST_CASE("fetch sends a buffer body and defaults the timeout",
          "[scripting][fetch]")
{
    MockNetwork network;
    ScriptingTest test(R"(
        local body = buffer.create(3)
        buffer.writeu8(body, 0, 1)
        buffer.writeu8(body, 1, 2)
        buffer.writeu8(body, 2, 255)
        fetch("https://example.com/upload", { method = "PUT", body = body })
        return 0
    )");
    REQUIRE(network.provider->starts.size() == 1);
    const scriptnet::HttpRequest& request = network.provider->starts[0].request;
    CHECK(request.method == "PUT");
    CHECK(request.body == std::vector<uint8_t>{1, 2, 255});
    CHECK(request.timeoutMs == scriptnet::NetLimits().defaultTimeoutMs);
}

TEST_CASE("fetch resolves with a Response", "[scripting][fetch]")
{
    MockNetwork network;
    ScriptingTest test(R"(
        local state = {}
        async(function()
            local _, response = await(fetch("https://example.com/"))
            state.status = response.status
            state.statusText = response.statusText
            state.ok = response.ok
            state.url = response.url
            local _, text = await(response:text())
            state.text = text
            local _, bytes = await(response:arrayBuffer())
            state.bytes = buffer.len(bytes)
            state.firstByte = buffer.readu8(bytes, 0)
            state.contentType = response:header("CONTENT-TYPE")
            state.missing = response:header("x-missing") == nil
            state.joined = response:header("x-multi")
            state.table = response.headers["x-multi"]
            state.lowered = response.headers["content-type"]
            return nil
        end)
        return state
    )");
    ScriptState state(test.state());
    REQUIRE(network.provider->starts.size() == 1);
    scriptnet::HttpResponse response = textResponse(200, "hello");
    response.headers = {{"Content-Type", "text/plain"},
                        {"X-Multi", "a"},
                        {"x-multi", "b"}};
    scriptnet::complete(network.provider->starts[0].id, std::move(response));
    rive_pollAsyncWork();

    CHECK(state.number("status") == 200);
    CHECK(state.string("statusText") == "OK");
    CHECK(state.boolean("ok"));
    CHECK(state.string("url") == "https://example.com/");
    CHECK(state.string("text") == "hello");
    CHECK(state.number("bytes") == 5);
    CHECK(state.number("firstByte") == 'h');
    CHECK(state.string("contentType") == "text/plain");
    CHECK(state.boolean("missing"));
    CHECK(state.string("joined") == "a, b");
    CHECK(state.string("table") == "a, b");
    CHECK(state.string("lowered") == "text/plain");
}

TEST_CASE("a non-2xx Response still resolves, with ok false",
          "[scripting][fetch]")
{
    MockNetwork network;
    ScriptingTest test(R"(
        local state = {}
        fetch("https://example.com/missing"):andThen(function(response)
            state.status = response.status
            state.ok = response.ok
        end, function(err)
            state.error = err
        end)
        return state
    )");
    ScriptState state(test.state());
    REQUIRE(network.provider->starts.size() == 1);
    scriptnet::complete(network.provider->starts[0].id,
                        textResponse(404, "nope"));
    rive_pollAsyncWork();
    CHECK(state.number("status") == 404);
    CHECK_FALSE(state.boolean("ok"));
    CHECK(state.string("error") == "<nil>");
}

TEST_CASE("fetch rejects with disabled when the host installed no transport",
          "[scripting][fetch]")
{
    scriptnet::setProvider(nullptr);
    ScriptingTest test(R"(
        local state = {}
        fetch("https://example.com/"):catch(function(err)
            state.error = err
        end)
        return state
    )");
    ScriptState state(test.state());
    CHECK(state.string("error") == "<nil>");
    rive_pollAsyncWork();
    CHECK(state.string("error") ==
          "disabled: network access is not enabled for scripts in this host");
}

TEST_CASE("fetch rejects policy refusals without reaching the provider",
          "[scripting][fetch]")
{
    MockNetwork network;
    ScriptingTest test(R"(
        local state = {}
        fetch("http://example.com/"):catch(function(err)
            state.scheme = err
        end)
        fetch("https://api.rive.app/api/me"):catch(function(err)
            state.rive = err
        end)
        fetch("https://example.com/", { headers = { Cookie = "a=b" } })
            :catch(function(err)
                state.cookie = err
            end)
        return state
    )");
    ScriptState state(test.state());
    rive_pollAsyncWork();
    CHECK(network.provider->starts.empty());
    CHECK(state.string("scheme") ==
          "policy: http: URLs are not allowed; use https:");
    CHECK(state.string("rive") ==
          "policy: Rive's own hosts are not reachable from scripts");
    CHECK(state.string("cookie") ==
          "policy: scripts cannot set the 'Cookie' header");
}

TEST_CASE("fetch rejects past the in-flight limit", "[scripting][fetch]")
{
    MockNetwork network;
    ScriptingTest test(R"(
        local state = { rejected = 0 }
        for i = 1, 9 do
            fetch("https://example.com/" .. i):catch(function(err)
                state.rejected += 1
                state.error = err
            end)
        end
        return state
    )");
    ScriptState state(test.state());
    CHECK(network.provider->starts.size() == 8);
    rive_pollAsyncWork();
    CHECK(state.number("rejected") == 1);
    CHECK(state.string("error") ==
          "rateLimited: more than 8 requests in flight");
}

TEST_CASE("fetch rejects past the per-minute limit", "[scripting][fetch]")
{
    MockNetwork network;
    scriptnet::NetLimits limits;
    limits.maxRequestsPerMinutePerOwner = 2;
    scriptnet::setLimits(limits);
    ScriptingTest test(R"(
        local state = {}
        for i = 1, 3 do
            fetch("https://example.com/" .. i):catch(function(err)
                state.error = err
            end)
        end
        return state
    )");
    ScriptState state(test.state());
    CHECK(network.provider->starts.size() == 2);
    rive_pollAsyncWork();
    CHECK(state.string("error") ==
          "rateLimited: more than 2 requests in the last minute");
}

TEST_CASE("fetch rejects a response over the size limit", "[scripting][fetch]")
{
    MockNetwork network;
    scriptnet::NetLimits limits;
    limits.maxResponseBodyBytes = 4;
    scriptnet::setLimits(limits);
    ScriptingTest test(R"(
        local state = {}
        fetch("https://example.com/"):catch(function(err)
            state.error = err
        end)
        return state
    )");
    ScriptState state(test.state());
    REQUIRE(network.provider->starts.size() == 1);
    scriptnet::complete(network.provider->starts[0].id,
                        textResponse(200, "hello"));
    rive_pollAsyncWork();
    CHECK(state.string("error") ==
          "tooLarge: the response body is larger than 4 bytes");
}

TEST_CASE("fetch rejects transport failures with their code",
          "[scripting][fetch]")
{
    MockNetwork network;
    ScriptingTest test(R"(
        local state = {}
        fetch("https://example.com/"):catch(function(err)
            state.error = err
        end)
        return state
    )");
    ScriptState state(test.state());
    REQUIRE(network.provider->starts.size() == 1);
    scriptnet::fail(network.provider->starts[0].id,
                    {scriptnet::NetErrorCode::Network, "connection reset"});
    // A second outcome for the same request is ignored.
    scriptnet::complete(network.provider->starts[0].id,
                        textResponse(200, "late"));
    rive_pollAsyncWork();
    CHECK(state.string("error") == "network: connection reset");
}

TEST_CASE("cancelling a fetch aborts it and it never settles",
          "[scripting][fetch]")
{
    MockNetwork network;
    ScriptingTest test(R"(
        local state = { settled = false }
        local promise = fetch("https://example.com/")
        promise:andThen(function() state.settled = true end,
                        function() state.settled = true end)
        promise:cancel()
        state.status = promise:getStatus()
        return state
    )");
    ScriptState state(test.state());
    REQUIRE(network.provider->starts.size() == 1);
    scriptnet::RequestId id = network.provider->starts[0].id;
    CHECK(network.provider->aborts == std::vector<scriptnet::RequestId>{id});
    CHECK(state.string("status") == "Cancelled");
    CHECK_FALSE(scriptnet::hasPendingWork());

    scriptnet::complete(id, textResponse(200, "late"));
    rive_pollAsyncWork();
    CHECK_FALSE(state.boolean("settled"));
}

TEST_CASE("a fetch cancelled by another's handler in the same poll never "
          "settles",
          "[scripting][fetch]")
{
    MockNetwork network;
    ScriptingTest test(R"(
        local state = { secondSettled = false }
        local second
        fetch("https://example.com/a"):andThen(function()
            second:cancel()
        end)
        second = fetch("https://example.com/b")
        second:andThen(function() state.secondSettled = true end,
                       function() state.secondSettled = true end)
        return state
    )");
    ScriptState state(test.state());
    REQUIRE(network.provider->starts.size() == 2);
    scriptnet::complete(network.provider->starts[0].id, textResponse(200, "a"));
    scriptnet::complete(network.provider->starts[1].id, textResponse(200, "b"));
    rive_pollAsyncWork();
    CHECK_FALSE(state.boolean("secondSettled"));
    CHECK_FALSE(scriptnet::hasPendingWork());

    // A registry ref freed twice would hand out the same slot twice.
    lua_State* L = test.state();
    lua_pushnumber(L, 1);
    int first = lua_ref(L, -1);
    lua_pushnumber(L, 2);
    int next = lua_ref(L, -1);
    lua_pop(L, 2);
    CHECK(first != next);
    lua_unref(L, first);
    lua_unref(L, next);
}

TEST_CASE("closing the VM aborts its outstanding fetches", "[scripting][fetch]")
{
    MockNetwork network;
    scriptnet::RequestId id = 0;
    {
        ScriptingTest test(R"(
            fetch("https://example.com/")
            return 0
        )");
        REQUIRE(network.provider->starts.size() == 1);
        id = network.provider->starts[0].id;
        CHECK(scriptnet::hasPendingWork());
    }
    CHECK(network.provider->aborts == std::vector<scriptnet::RequestId>{id});
    CHECK_FALSE(scriptnet::hasPendingWork());
    // The transport finishing after the VM is gone is a no-op.
    scriptnet::complete(id, textResponse(200, "late"));
    CHECK(scriptnet::poll() == 0);
}

TEST_CASE("the watchdog times out a provider that never answers",
          "[scripting][fetch]")
{
    MockNetwork network;
    scriptnet::NetLimits limits;
    limits.watchdogGraceMs = 0;
    scriptnet::setLimits(limits);
    ScriptingTest test(R"(
        local state = {}
        fetch("https://example.com/", { timeout = 1 }):catch(function(err)
            state.error = err
        end)
        return state
    )");
    ScriptState state(test.state());
    REQUIRE(network.provider->starts.size() == 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    rive_pollAsyncWork();
    CHECK(state.string("error") == "timeout: no response before the timeout");
    CHECK(network.provider->aborts ==
          std::vector<scriptnet::RequestId>{network.provider->starts[0].id});
}

TEST_CASE("await on fetch gives ok with the Response or the error",
          "[scripting][fetch]")
{
    MockNetwork network;
    ScriptingTest test(R"(
        local state = { done = false }
        async(function()
            local ok, response = await(fetch("https://example.com/"))
            state.firstOk = ok
            state.status = response.status
            local refusedOk, err = await(fetch("http://example.com/"))
            state.secondOk = refusedOk
            state.error = err
            state.done = true
        end)
        return state
    )");
    ScriptState state(test.state());
    REQUIRE(network.provider->starts.size() == 1);
    scriptnet::complete(network.provider->starts[0].id,
                        textResponse(200, "hi"));
    rive_pollAsyncWork();
    CHECK(state.boolean("firstOk"));
    CHECK(state.number("status") == 200);
    CHECK_FALSE(state.boolean("done"));
    // The refusal queued while the first delivery ran; the next poll
    // delivers it.
    rive_pollAsyncWork();
    CHECK(state.boolean("done"));
    CHECK_FALSE(state.boolean("secondOk"));
    CHECK(state.string("error") ==
          "policy: http: URLs are not allowed; use https:");
}

TEST_CASE("fetch delivers only on the thread that started it",
          "[scripting][fetch]")
{
    MockNetwork network;
    ScriptingTest test(R"(
        local state = { settled = false }
        fetch("https://example.com/"):andThen(function()
            state.settled = true
        end)
        return state
    )");
    ScriptState state(test.state());
    REQUIRE(network.provider->starts.size() == 1);
    scriptnet::complete(network.provider->starts[0].id,
                        textResponse(200, "hi"));

    uint32_t deliveredElsewhere = 1;
    bool pendingElsewhere = true;
    std::thread other([&] {
        deliveredElsewhere = scriptnet::poll();
        pendingElsewhere = scriptnet::hasPendingWork();
    });
    other.join();
    CHECK(deliveredElsewhere == 0);
    CHECK_FALSE(pendingElsewhere);
    CHECK_FALSE(state.boolean("settled"));

    rive_pollAsyncWork();
    CHECK(state.boolean("settled"));
}

TEST_CASE("fetch raises on malformed arguments", "[scripting][fetch]")
{
    MockNetwork network;
    auto errorOf = [](const char* source) {
        ScriptingTest test(source, 1, /*errorOk=*/true);
        const char* error = lua_tostring(test.state(), -1);
        return std::string(error != nullptr ? error : "");
    };
    CHECK(errorOf(R"(fetch("https://example.com/", { headers = { A = 1 } }))")
              .find("fetch: options.headers must map header names to string "
                    "values") != std::string::npos);
    CHECK(errorOf(R"(fetch("https://example.com/", { method = 1 }))")
              .find("fetch: options.method must be a string") !=
          std::string::npos);
    CHECK(errorOf(R"(fetch("https://example.com/", { body = {} }))")
              .find("fetch: options.body must be a string or a buffer") !=
          std::string::npos);
    CHECK(errorOf(R"(fetch("https://example.com/", { timeout = -1 }))")
              .find("fetch: options.timeout must be a non-negative number") !=
          std::string::npos);
    CHECK(errorOf(R"(fetch("https://example.com/", "GET"))")
              .find("table expected") != std::string::npos);
    CHECK(errorOf(R"(fetch())").find("string expected") != std::string::npos);
    CHECK(network.provider->starts.empty());
}

namespace
{
class RecordingListener : public scriptnet::FetchListener
{
public:
    bool cancelled = false;
    void onResponse(scriptnet::HttpResponse&&) override {}
    void onError(const scriptnet::NetError&) override {}
    void onCancel() override { cancelled = true; }
};

scriptnet::HttpRequest getRequest()
{
    scriptnet::HttpRequest request;
    request.url = "https://example.com/";
    return request;
}

// A transport whose start() waits for the test to let it go, standing in for
// one slow to take a request while another thread cancels it.
class BlockingProvider : public scriptnet::Provider
{
public:
    std::mutex mutex;
    std::condition_variable changed;
    scriptnet::RequestId entered = 0;
    bool released = false;
    std::vector<std::string> events;

    void start(scriptnet::RequestId id, const scriptnet::HttpRequest&) override
    {
        std::unique_lock<std::mutex> lock(mutex);
        entered = id;
        changed.notify_all();
        changed.wait(lock, [this] { return released; });
        events.push_back("start");
    }
    void abort(scriptnet::RequestId) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        events.push_back("abort");
    }
};
} // namespace

TEST_CASE("a request aborts through the transport that started it",
          "[scripting][fetch]")
{
    MockNetwork network;
    auto listener = make_rcp<RecordingListener>();
    scriptnet::RequestId id = scriptnet::fetch(1, getRequest(), listener);
    REQUIRE(network.provider->starts.size() == 1);
    // A new transport takes over; the request in flight is still the first
    // one's to abort.
    auto replacement = make_rcp<MockProvider>();
    scriptnet::setProvider(replacement);
    scriptnet::cancel(id);
    CHECK(network.provider->aborts == std::vector<scriptnet::RequestId>{id});
    CHECK(replacement->aborts.empty());
    CHECK(listener->cancelled);
}

TEST_CASE("a cancel while the transport starts a request aborts it after",
          "[scripting][fetch]")
{
    auto provider = make_rcp<BlockingProvider>();
    scriptnet::setProvider(provider);
    scriptnet::setLimits(scriptnet::NetLimits());
    auto listener = make_rcp<RecordingListener>();
    std::thread fetching(
        [&listener] { scriptnet::fetch(1, getRequest(), listener); });
    scriptnet::RequestId id = 0;
    {
        std::unique_lock<std::mutex> lock(provider->mutex);
        provider->changed.wait(lock, [&] { return provider->entered != 0; });
        id = provider->entered;
    }
    // The transport has not taken the request yet: aborting it now would
    // miss, and the start that follows would run untracked.
    scriptnet::cancel(id);
    CHECK(listener->cancelled);
    {
        std::lock_guard<std::mutex> lock(provider->mutex);
        CHECK(provider->events.empty());
        provider->released = true;
    }
    provider->changed.notify_all();
    fetching.join();
    CHECK(provider->events == std::vector<std::string>{"start", "abort"});
    scriptnet::setProvider(nullptr);
}

#endif // WITH_RIVE_SCRIPTNET
