#ifdef WITH_RIVE_SCRIPTNET

#include "catch.hpp"
#include "rive/scriptnet/net.hpp"

#include <chrono>
#include <functional>
#include <thread>
#include <string>
#include <utility>
#include <vector>

using namespace rive;
using namespace rive::scriptnet;

namespace
{
const uint64_t kOwner = 77;
const char* kBase = "http://127.0.0.1:8700/v1/ns/ns_test";

// Each test runs four ways: for a listener that takes its body whole and one
// that reads it as it arrives, from a transport that reports a response
// whole and one that reports it in steps. Host sets these.
bool g_listenerStreams = false;
bool g_transportSteps = false;

class Transport : public Provider
{
public:
  struct Start
  {
    RequestId id;
    HttpRequest request;
  };
  std::vector<Start> starts;
  // Runs once, inside the next start().
  std::function<void()> duringStart;
  void start(RequestId id, const HttpRequest& request) override
  {
    starts.push_back({id, request});
    if (auto hook = std::exchange(duringStart, nullptr))
    {
      hook();
    }
  }
};

class Result : public FetchListener
{
public:
  RequestId id = 0;
  bool settled = false;
  uint16_t status = 0;
  std::string body;
  std::string url;
  std::string error;
  bool streams() const override { return g_listenerStreams; }
  void onResponse(HttpResponse&& response) override
  {
    settled = true;
    status = response.status;
    url = response.url;
    if (!g_listenerStreams)
    {
      body.assign(response.body.begin(), response.body.end());
      return;
    }
    // The host read the answer whole, so all of it is there to read, and
    // none of it came with the response.
    CHECK(response.body.empty());
    std::vector<uint8_t> bytes;
    NetError failure;
    while (readBody(id, &bytes, &failure) == BodyState::Data)
    {
      body.append(bytes.begin(), bytes.end());
    }
    CHECK(failure.message.empty());
  }
  void onError(const NetError& failure) override
  {
    settled = true;
    error = std::string(netErrorCodeName(failure.code)) + ": " +
            failure.message;
  }
};

std::string header(const HttpRequest& request, const char* name)
{
  for (const HttpHeader& each : request.headers)
  {
    if (lowerAscii(each.name) == name)
    {
      return each.value;
    }
  }
  return "";
}

std::string text(const HttpRequest& request)
{
  return std::string(request.body.begin(), request.body.end());
}

// A host with an endpoint, a device key that signs anything as "SIG", and a
// transport the test answers by hand.
struct Host
{
  rcp<Transport> transport = make_rcp<Transport>();
  std::vector<std::string> signedMessages;
  // Answered so far, so a test reads the starts in order.
  size_t next = 0;

  Host()
  {
    g_listenerStreams = GENERATE(false, true);
    g_transportSteps = GENERATE(false, true);
    setProvider(transport);
    setLimits(NetLimits());
    setGeneralFetchEnabled(false);
    CloudEndpoint endpoint;
    endpoint.baseUrl = "http://127.0.0.1:8700/";
    endpoint.namespaceId = "ns_test";
    endpoint.installId = "install-1";
    endpoint.signHello =
        [this](const std::string& message, CloudEndpoint::HelloSigned done) {
          signedMessages.push_back(message);
          done(true, "{\"kty\":\"EC\"}", "SIG");
        };
    setCloudEndpoint(std::move(endpoint));
  }
  ~Host()
  {
    cancelAllForOwner(kOwner);
    setCloudEndpoint(CloudEndpoint());
    setGeneralFetchEnabled(true);
    setProvider(nullptr);
  }

  rcp<Result> send(
      const char* method,
      const char* path,
      const char* body = "",
      HttpHeaders headers = {})
  {
    return sendTo(0, method, path, body, std::move(headers));
  }

  // As send(), for a file that added its own endpoint. Another file is
  // another VM, so another owner, whose queue no other file's waits in.
  rcp<Result> sendTo(
      CloudEndpointId cloud,
      const char* method,
      const char* path,
      const char* body = "",
      HttpHeaders headers = {})
  {
    auto result = make_rcp<Result>();
    HttpRequest request;
    request.method = method;
    request.url = std::string("rive-relay:") + path;
    request.headers = std::move(headers);
    request.body.assign(body, body + strlen(body));
    result->id = fetch(kOwner + cloud, std::move(request), result, cloud);
    return result;
  }

  // The start sent to exactly `url`, which must exist.
  size_t indexOf(const std::string& url)
  {
    for (size_t i = 0; i < transport->starts.size(); i++)
    {
      if (transport->starts[i].request.url == url)
      {
        return i;
      }
    }
    FAIL("nothing was sent to " << url);
    return 0;
  }

  // The next request the transport was handed, which must exist.
  const HttpRequest& sent()
  {
    REQUIRE(transport->starts.size() > next);
    return transport->starts[next].request;
  }

  void answer(uint16_t status, const std::string& body)
  {
    answerAt(next++, status, body);
  }

  // Answers one out of turn, as a slow request's answer arrives.
  void answerAt(size_t index, uint16_t status, const std::string& body)
  {
    REQUIRE(transport->starts.size() > index);
    HttpResponse response;
    response.status = status;
    // As a transport reports it: the address the request went to.
    response.url = transport->starts[index].request.url;
    RequestId id = transport->starts[index].id;
    if (g_transportSteps)
    {
      respond(id, std::move(response));
      receive(id, (const uint8_t*)body.data(), body.size());
      finish(id);
    }
    else
    {
      response.body.assign(body.begin(), body.end());
      complete(id, std::move(response));
    }
    poll();
  }

  // Names another endpoint, whose device key signs as this one's does.
  void moveTo(const char* baseUrl, const char* namespaceId)
  {
    CloudEndpoint endpoint;
    endpoint.baseUrl = baseUrl;
    endpoint.namespaceId = namespaceId;
    endpoint.signHello =
        [this](const std::string& message, CloudEndpoint::HelloSigned done) {
          signedMessages.push_back(message);
          done(true, "{}", "S");
        };
    setCloudEndpoint(std::move(endpoint));
  }

  // Answers the challenge and the hello a first request sets off.
  void sayHello(const char* token = "tok.A_1")
  {
    CHECK(sent().url == std::string(kBase) + "/hello/challenge");
    answer(200, "{\"challenge\":\"v1.abc\",\"expires_in\":60}");
    CHECK(sent().url == std::string(kBase) + "/hello");
    answer(
        200,
        std::string("{\"token\":\"") + token +
            "\",\"player_id\":\"p_one\",\"display_name\":\"Ada\"}");
  }
};
} // namespace

TEST_CASE(
    "a rive-cloud request waits for the host's hello and carries its token",
    "[scriptnet][cloud]")
{
  Host host;
  auto result = host.send("GET", "/v1/players/me/data/save");
  poll();
  // Only the challenge is out: the script's request waits.
  REQUIRE(host.transport->starts.size() == 1);
  CHECK(host.sent().method == "POST");
  host.answer(200, "{\"challenge\":\"v1.abc\",\"expires_in\":60}");

  REQUIRE(host.transport->starts.size() == 2);
  CHECK(
      host.signedMessages == std::vector<std::string>{"rive-hello-v1\nv1.abc"});
  CHECK(
      text(host.sent()) ==
      "{\"challenge\":\"v1.abc\",\"device\":{\"key\":{\"kty\":\"EC\"},"
      "\"sig\":\"SIG\"},\"install_id\":\"install-1\"}");
  host.answer(200, "{\"token\":\"tok.A_1\",\"player_id\":\"p_one\"}");

  REQUIRE(host.transport->starts.size() == 3);
  CHECK(host.sent().url == std::string(kBase) + "/players/me/data/save");
  CHECK(header(host.sent(), "authorization") == "Bearer tok.A_1");
  // The transport sends all three as the host's own, not a script's fetch.
  for (const auto& start : host.transport->starts)
  {
    CHECK(start.request.hostChosen);
  }
  CHECK_FALSE(result->settled);
  host.answer(200, "{\"value\":1}");
  CHECK(result->status == 200);
  CHECK(result->body == "{\"value\":1}");

  // The next one needs no hello.
  auto second = host.send("GET", "/v1/app/data/x");
  REQUIRE(host.transport->starts.size() == 4);
  CHECK(header(host.sent(), "authorization") == "Bearer tok.A_1");
  host.answer(404, "{}");
  CHECK(second->status == 404);
}

TEST_CASE(
    "a script's hello is answered by the host without the token",
    "[scriptnet][cloud]")
{
  Host host;
  auto who = host.send("POST", "/v1/hello", "{}");
  poll();
  host.sayHello();
  poll();
  REQUIRE(who->settled);
  CHECK(who->status == 200);
  CHECK(who->body == "{\"player_id\":\"p_one\",\"display_name\":\"Ada\"}");
  // Two calls, the host's own. The script's was never sent.
  CHECK(host.transport->starts.size() == 2);

  // Asking again costs nothing, and the same name changes nothing.
  auto again = host.send("POST", "/v1/hello", "{\"display_name\":\"Ada\"}");
  // One poll answers it from the queue and the next delivers the answer.
  poll();
  poll();
  CHECK(again->status == 200);
  CHECK(host.transport->starts.size() == 2);

  // A new name is a new hello, which carries it.
  auto renamed = host.send("POST", "/v1/hello", "{\"display_name\":\"Bo\"}");
  poll();
  REQUIRE(host.transport->starts.size() == 3);
  host.answer(200, "{\"challenge\":\"v1.def\"}");
  CHECK(
      text(host.sent()).find(",\"display_name\":\"Bo\"}") != std::string::npos);
  host.answer(
      200,
      "{\"token\":\"tok.B\",\"player_id\":\"p_one\",\"display_name\":\"Bo\"}");
  poll();
  CHECK(renamed->body == "{\"player_id\":\"p_one\",\"display_name\":\"Bo\"}");
}

TEST_CASE(
    "a name chosen while a hello is out is sent by the next one",
    "[scriptnet][cloud]")
{
  Host host;
  auto data = host.send("GET", "/v1/app/data/a");
  poll();
  host.answer(200, "{\"challenge\":\"v1.abc\"}");
  // The hello is out, without a name, when the script chooses one.
  CHECK(text(host.sent()).find("display_name") == std::string::npos);
  auto renamed = host.send("POST", "/v1/hello", "{\"display_name\":\"Bo\"}");
  host.answer(200, "{\"token\":\"tok.A\",\"player_id\":\"p_one\"}");
  // That token is not used: nothing was sent with it and nobody was told.
  CHECK_FALSE(data->settled);
  CHECK_FALSE(renamed->settled);
  host.answer(200, "{\"challenge\":\"v1.def\"}");
  CHECK(
      text(host.sent()).find(",\"display_name\":\"Bo\"}") != std::string::npos);
  host.answer(
      200,
      "{\"token\":\"tok.B\",\"player_id\":\"p_one\",\"display_name\":\"Bo\"}");
  CHECK(header(host.sent(), "authorization") == "Bearer tok.B");
  host.answer(200, "{}");
  poll();
  CHECK(data->status == 200);
  CHECK(renamed->body == "{\"player_id\":\"p_one\",\"display_name\":\"Bo\"}");
}

TEST_CASE(
    "a response names the rive-cloud URL, never the engine's address",
    "[scriptnet][cloud]")
{
  Host host;
  auto who = host.send("POST", "/v1/hello", "{}");
  auto data = host.send("GET", "/v1/app/data/a");
  poll();
  host.sayHello();
  host.answer(200, "{}");
  poll();
  CHECK(who->url == "rive-relay:/v1/hello");
  CHECK(data->url == "rive-relay:/v1/app/data/a");

  // A refused hello's answer reaches the script under its own URL too.
  setCloudEndpoint(CloudEndpoint());
  Host again;
  auto refused = again.send("GET", "/v1/app/data/b");
  poll();
  again.answer(503, "{}");
  CHECK(refused->status == 503);
  CHECK(refused->url == "rive-relay:/v1/app/data/b");
}

TEST_CASE(
    "a name the engine refuses does not fail every later hello",
    "[scriptnet][cloud]")
{
  Host host;
  auto named = host.send("POST", "/v1/hello", "{\"display_name\":\"Bad\"}");
  poll();
  host.answer(200, "{\"challenge\":\"v1.abc\"}");
  host.answer(400, "{\"error\":\"bad_request\"}");
  CHECK(named->status == 400);
  // The next request says hello without the name, and goes through.
  auto data = host.send("GET", "/v1/app/data/a");
  poll();
  host.answer(200, "{\"challenge\":\"v1.def\"}");
  CHECK(text(host.sent()).find("display_name") == std::string::npos);
  host.answer(200, "{\"token\":\"tok.A\",\"player_id\":\"p_one\"}");
  host.answer(200, "{}");
  CHECK(data->status == 200);
}

TEST_CASE(
    "a 401 is answered with a new hello and the request sent again, once",
    "[scriptnet][cloud]")
{
  Host host;
  auto result = host.send("PUT", "/v1/players/me/data/save", "{\"value\":2}");
  poll();
  host.sayHello();
  CHECK(header(host.sent(), "authorization") == "Bearer tok.A_1");
  // An expired token's 401 carries the next challenge, so no challenge call.
  host.answer(
      401,
      "{\"error\":\"unauthorized\",\"message\":\"expired\","
      "\"challenge\":\"v1.next\"}");
  CHECK_FALSE(result->settled);
  CHECK(host.sent().url == std::string(kBase) + "/hello");
  CHECK(host.signedMessages.back() == "rive-hello-v1\nv1.next");
  host.answer(200, "{\"token\":\"tok.B\",\"player_id\":\"p_one\"}");
  CHECK(header(host.sent(), "authorization") == "Bearer tok.B");
  CHECK(text(host.sent()) == "{\"value\":2}");
  CHECK(host.sent().hostChosen);
  // A second 401 is the script's to see.
  host.answer(401, "{\"error\":\"unauthorized\"}");
  CHECK(result->status == 401);
}

TEST_CASE(
    "a 401 with no challenge gets one new hello, not one for every call",
    "[scriptnet][cloud]")
{
  Host host;
  const char* refusal =
      "{\"error\":\"unauthorized\",\"message\":\"bad namespace secret\"}";
  auto first = host.send("GET", "/v1/server/players/p_two/data/save");
  poll();
  host.sayHello();
  host.answer(401, refusal);
  // The engine may no longer take the token, so the host says hello again,
  // asking for a challenge since the 401 gave none.
  CHECK_FALSE(first->settled);
  host.sayHello("tok.B");
  CHECK(header(host.sent(), "authorization") == "Bearer tok.B");
  host.answer(401, refusal);
  CHECK(first->status == 401);

  // It was the route. Asking again costs no hello and leaves the token.
  auto second = host.send("GET", "/v1/server/players/p_two/data/save");
  REQUIRE(host.transport->starts.size() == 7);
  host.answer(401, refusal);
  CHECK(second->status == 401);
  auto next = host.send("GET", "/v1/app/data/a");
  REQUIRE(host.transport->starts.size() == 8);
  CHECK(header(host.sent(), "authorization") == "Bearer tok.B");
  host.answer(200, "{}");
  CHECK(next->status == 200);
}

TEST_CASE(
    "a refused hello never reads as a waiting request's own answer",
    "[scriptnet][cloud]")
{
  Host host;
  auto who = host.send("POST", "/v1/hello", "{}");
  auto data = host.send("GET", "/v1/app/data/a");
  poll();
  host.answer(404, "{\"error\":\"not_found\"}");
  // The hello's status is the answer to a script's hello, and to nothing else.
  CHECK(who->status == 404);
  CHECK(data->status == 401);
  CHECK(data->body == "{\"error\":\"not_found\"}");
  CHECK(data->url == "rive-relay:/v1/app/data/a");

  // Too many hellos is too many requests, for either.
  auto limited = host.send("GET", "/v1/app/data/b");
  poll();
  host.answer(429, "{\"error\":\"rate_limited\"}");
  CHECK(limited->status == 429);
}

TEST_CASE(
    "a hello answer the host cannot use reaches no script",
    "[scriptnet][cloud]")
{
  Host host;
  auto who = host.send("POST", "/v1/hello", "{}");
  auto data = host.send("GET", "/v1/app/data/a");
  poll();
  host.answer(200, "{\"challenge\":\"v1.abc\"}");
  // A token with a character the host does not take.
  host.answer(200, "{\"token\":\"tok+A\",\"player_id\":\"p_one\"}");
  CHECK(who->error.rfind("network:", 0) == 0);
  CHECK(data->error.rfind("network:", 0) == 0);
  CHECK(who->body.empty());
  CHECK(data->body.empty());

  // A page that is no challenge, as a proxy might answer.
  auto later = host.send("POST", "/v1/hello", "{}");
  poll();
  host.answer(200, "<html>sign in</html>");
  CHECK(later->error.rfind("network:", 0) == 0);
}

TEST_CASE(
    "requests made for one endpoint never carry the next one's token",
    "[scriptnet][cloud]")
{
  Host host;
  auto sent = host.send("GET", "/v1/app/data/a");
  poll();
  host.sayHello();
  // One is out with the first endpoint's token and one waits behind a new
  // name's hello when the host names another endpoint.
  auto renamed = host.send("POST", "/v1/hello", "{\"display_name\":\"Bo\"}");
  auto waiting = host.send("GET", "/v1/app/data/b");
  REQUIRE(host.transport->starts.size() == 4);
  host.moveTo("http://127.0.0.1:9900", "ns_other");
  poll();
  CHECK(renamed->error.rfind("aborted:", 0) == 0);
  CHECK(waiting->error.rfind("aborted:", 0) == 0);

  auto fresh = host.send("GET", "/v1/app/data/c");
  poll();
  REQUIRE(host.transport->starts.size() == 5);
  // The first endpoint's answer to the request already out comes last.
  size_t out = host.next++;
  // Its challenge for the name's hello is dropped.
  host.answer(200, "{\"challenge\":\"v1.stale\"}");
  const char* other = "http://127.0.0.1:9900/v1/ns/ns_other";
  CHECK(host.sent().url == std::string(other) + "/hello/challenge");
  host.answer(200, "{\"challenge\":\"v1.new\"}");
  CHECK(host.signedMessages.back() == "rive-hello-v1\nv1.new");
  host.answer(200, "{\"token\":\"tok.C\",\"player_id\":\"p_two\"}");
  CHECK(host.sent().url == std::string(other) + "/app/data/c");
  CHECK(header(host.sent(), "authorization") == "Bearer tok.C");
  host.answer(200, "{}");
  CHECK(fresh->status == 200);
  // The first endpoint says its token expired. That is the script's to see:
  // the request is not sent again, and the new endpoint's token stays.
  host.answerAt(
      out,
      401,
      "{\"error\":\"unauthorized\",\"challenge\":\"v1.old\"}");
  CHECK(sent->status == 401);
  auto after = host.send("GET", "/v1/app/data/d");
  REQUIRE(host.transport->starts.size() == 8);
  CHECK(header(host.sent(), "authorization") == "Bearer tok.C");
  host.answer(200, "{}");
  CHECK(after->status == 200);

  // With no endpoint at all, what waits is refused rather than left waiting.
  auto stranded = host.send("POST", "/v1/hello", "{\"display_name\":\"Cy\"}");
  setCloudEndpoint(CloudEndpoint());
  poll();
  CHECK(stranded->error.rfind("aborted:", 0) == 0);
  // The challenge its name had asked for.
  host.answer(200, "{\"challenge\":\"v1.late\"}");
  CHECK(host.transport->starts.size() == 9);
}

TEST_CASE(
    "a request readied for one endpoint is not sent once there is another",
    "[scriptnet][cloud]")
{
  Host host;
  auto first = host.send("PUT", "/v1/app/data/a", "{}");
  auto second = host.send("PUT", "/v1/app/data/b", "{}");
  poll();
  host.answer(200, "{\"challenge\":\"v1.abc\"}");
  // The token readies both, and the host names another endpoint while the
  // transport is handed the first.
  host.transport->duringStart = [&host] {
    host.moveTo("http://127.0.0.1:9900", "ns_other");
  };
  host.answer(200, "{\"token\":\"tok.A\",\"player_id\":\"p_one\"}");
  REQUIRE(host.transport->starts.size() == 3);
  CHECK(host.sent().url == std::string(kBase) + "/app/data/a");
  poll();
  CHECK(second->error.rfind("aborted:", 0) == 0);
  // The one already handed over is answered as any other.
  host.answer(200, "{}");
  CHECK(first->status == 200);
  CHECK(host.transport->starts.size() == 3);
}

TEST_CASE(
    "a key may hold dots, and no segment is a dot segment",
    "[scriptnet][cloud]")
{
  Host host;
  auto parent = host.send("GET", "/v1/players/../x");
  auto current = host.send("POST", "/v1/./hello/challenge");
  auto encoded = host.send("GET", "/v1/players/%2E%2e/x");
  auto mixed = host.send("GET", "/v1/players/me/data/.%2e");
  auto last = host.send("GET", "/v1/players/me/data/.");
  poll();
  CHECK(parent->error.rfind("policy:", 0) == 0);
  CHECK(current->error.rfind("policy:", 0) == 0);
  CHECK(encoded->error.rfind("policy:", 0) == 0);
  CHECK(mixed->error.rfind("policy:", 0) == 0);
  CHECK(last->error.rfind("policy:", 0) == 0);
  CHECK(host.transport->starts.empty());

  auto dotted = host.send("GET", "/v1/players/me/data/save..bak");
  auto hidden = host.send("GET", "/v1/players/me/data/...");
  auto listed = host.send("GET", "/v1/players/me/data?prefix=v1..&after=..");
  poll();
  host.sayHello();
  CHECK(host.sent().url == std::string(kBase) + "/players/me/data/save..bak");
  host.answer(200, "{}");
  CHECK(host.sent().url == std::string(kBase) + "/players/me/data/...");
  host.answer(200, "{}");
  CHECK(
      host.sent().url ==
      std::string(kBase) + "/players/me/data?prefix=v1..&after=..");
  host.answer(200, "{}");
  CHECK(dotted->status == 200);
  CHECK(hidden->status == 200);
  CHECK(listed->status == 200);
}

TEST_CASE(
    "a hello's 503 or transport error is what the waiting requests get",
    "[scriptnet][cloud]")
{
  Host host;
  auto first = host.send("GET", "/v1/app/data/a");
  auto second = host.send("GET", "/v1/app/data/b");
  poll();
  host.answer(503, "{\"error\":\"unavailable\"}");
  CHECK(first->status == 503);
  CHECK(second->status == 503);
  CHECK(host.transport->starts.size() == 1);

  // The next request starts over.
  auto third = host.send("GET", "/v1/app/data/c");
  poll();
  host.answer(200, "{\"challenge\":\"v1.abc\"}");
  fail(
      host.transport->starts[host.next++].id,
      {NetErrorCode::Network, "connection refused"});
  poll();
  CHECK(third->error == "network: connection refused");
}

TEST_CASE(
    "a challenge the engine refuses is signed once more",
    "[scriptnet][cloud]")
{
  Host host;
  auto result = host.send("GET", "/v1/app/data/a");
  poll();
  host.answer(200, "{\"challenge\":\"v1.one\"}");
  host.answer(
      401,
      "{\"error\":\"challenge_invalid\",\"challenge\":\"v1.two\"}");
  CHECK(host.signedMessages.back() == "rive-hello-v1\nv1.two");
  host.answer(
      401,
      "{\"error\":\"challenge_invalid\",\"challenge\":\"v1.three\"}");
  CHECK(result->status == 401);
  CHECK(host.signedMessages.size() == 2);
}

TEST_CASE(
    "a script cannot set the token, reach the hello calls or reuse a key",
    "[scriptnet][cloud]")
{
  Host host;
  auto challenge = host.send("POST", "/v1/hello/challenge");
  auto get = host.send("GET", "/v1/hello");
  // The engine would decode these to the hello routes.
  auto encoded = host.send("POST", "/v1/%68ello/challenge");
  auto encodedHello = host.send("POST", "/v1/hell%6f");
  auto fragment = host.send("POST", "/v1/hello#probe");
  poll();
  CHECK(fragment->error.rfind("policy:", 0) == 0);
  CHECK(challenge->error.rfind("policy:", 0) == 0);
  CHECK(get->error.rfind("policy:", 0) == 0);
  CHECK(encoded->error.rfind("policy:", 0) == 0);
  CHECK(encodedHello->error.rfind("policy:", 0) == 0);
  CHECK(host.transport->starts.empty());

  auto write = host.send(
      "PUT",
      "/v1/players/me/data/save",
      "{}",
      {{"Authorization", "Bearer mine"},
       {"Idempotency-Key", "7"},
       {"Content-Type", "text/plain"}});
  poll();
  host.sayHello();
  const HttpRequest& sent = host.sent();
  CHECK(header(sent, "authorization") == "Bearer tok.A_1");
  // The engine would read a text/plain body as a browser's wrapped request.
  CHECK(header(sent, "content-type") == "application/json");
  std::string key = header(sent, "idempotency-key");
  // A prefix for the run, then the owner, then the script's own key.
  REQUIRE(key.size() > 32);
  CHECK(key.substr(32) == ".77.7");
  host.answer(200, "{}");

  // Another endpoint is another run: its keys differ, and so does its hello.
  CloudEndpoint endpoint;
  endpoint.baseUrl = "http://127.0.0.1:8700";
  endpoint.namespaceId = "ns_test";
  endpoint.signHello = [](const std::string&, CloudEndpoint::HelloSigned done) {
    done(true, "{}", "S");
  };
  setCloudEndpoint(std::move(endpoint));
  auto later = host.send(
      "PUT",
      "/v1/players/me/data/save",
      "{}",
      {{"Idempotency-Key", "7"}});
  poll();
  host.sayHello("tok.C");
  CHECK(header(host.sent(), "idempotency-key") != key);
  CHECK(header(host.sent(), "authorization") == "Bearer tok.C");
  host.answer(200, "{}");
  CHECK(later->status == 200);
  CHECK(write->status == 200);
}

TEST_CASE(
    "a host with no device key refuses the request that needed a hello",
    "[scriptnet][cloud]")
{
  Host host;
  CloudEndpoint endpoint;
  endpoint.baseUrl = "http://127.0.0.1:8700";
  endpoint.namespaceId = "ns_test";
  setCloudEndpoint(std::move(endpoint));
  auto result = host.send("GET", "/v1/app/data/a");
  poll();
  host.answer(200, "{\"challenge\":\"v1.abc\"}");
  poll();
  CHECK(result->error.rfind("policy: this host has no device key", 0) == 0);
}

TEST_CASE(
    "a signature that comes later sends the hello, unless the endpoint changed",
    "[scriptnet][cloud]")
{
  Host host;
  std::vector<CloudEndpoint::HelloSigned> pending;
  auto signLater =
      [&pending](const std::string&, CloudEndpoint::HelloSigned done) {
        pending.push_back(std::move(done));
      };
  CloudEndpoint endpoint;
  endpoint.baseUrl = "http://127.0.0.1:8700";
  endpoint.namespaceId = "ns_test";
  endpoint.signHello = signLater;
  setCloudEndpoint(endpoint);
  auto result = host.send("GET", "/v1/app/data/a");
  poll();
  host.answer(200, "{\"challenge\":\"v1.abc\"}");
  poll();
  // Waiting on the key: nothing more is sent, however often it polls.
  REQUIRE(pending.size() == 1);
  CHECK(host.transport->starts.size() == 1);
  pending[0](true, "{}", "S");
  REQUIRE(host.transport->starts.size() == 2);
  CHECK(host.sent().url == std::string(kBase) + "/hello");
  host.answer(200, "{\"token\":\"tok.A\"}");
  host.answer(200, "{}");
  CHECK(result->status == 200);

  // A signature for an endpoint since replaced is dropped.
  setCloudEndpoint(endpoint);
  auto later = host.send("GET", "/v1/app/data/b");
  poll();
  host.answer(200, "{\"challenge\":\"v1.def\"}");
  poll();
  REQUIRE(pending.size() == 2);
  setCloudEndpoint(endpoint);
  pending[1](true, "{}", "S");
  CHECK(host.transport->starts.size() == host.next);
  poll();
  CHECK(later->error.rfind("aborted", 0) == 0);
}

TEST_CASE(
    "a device key that never signs fails the hello after the timeout",
    "[scriptnet][cloud]")
{
  Host host;
  NetLimits quick;
  quick.defaultTimeoutMs = 1;
  setLimits(quick);
  CloudEndpoint::HelloSigned kept;
  CloudEndpoint endpoint;
  endpoint.baseUrl = "http://127.0.0.1:8700";
  endpoint.namespaceId = "ns_test";
  endpoint.signHello =
      [&kept](const std::string&, CloudEndpoint::HelloSigned done) {
        kept = std::move(done);
      };
  setCloudEndpoint(std::move(endpoint));
  auto result = host.send("GET", "/v1/app/data/a");
  poll();
  host.answer(200, "{\"challenge\":\"v1.abc\"}");
  poll();
  REQUIRE(kept);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  poll();
  poll();
  CHECK(result->error.rfind("timeout: the device key did not sign", 0) == 0);
  // Its signature, when it comes at last, sends nothing.
  size_t sent = host.transport->starts.size();
  kept(true, "{}", "S");
  CHECK(host.transport->starts.size() == sent);
}

TEST_CASE(
    "a device key that cannot sign fails the hello, and the next request "
    "says hello again",
    "[scriptnet][cloud]")
{
  Host host;
  int signs = 0;
  CloudEndpoint endpoint;
  endpoint.baseUrl = "http://127.0.0.1:8700";
  endpoint.namespaceId = "ns_test";
  endpoint.signHello =
      [&signs](const std::string&, CloudEndpoint::HelloSigned done) {
        done(++signs > 1, "{}", "S");
      };
  setCloudEndpoint(std::move(endpoint));
  auto first = host.send("GET", "/v1/app/data/a");
  poll();
  host.answer(200, "{\"challenge\":\"v1.abc\"}");
  poll();
  CHECK(first->error == "policy: the device key could not sign the hello");

  auto second = host.send("GET", "/v1/app/data/b");
  poll();
  host.sayHello();
  host.answer(200, "{}");
  CHECK(second->status == 200);
  CHECK(signs == 2);
}

TEST_CASE(
    "an endpoint that would change a request's path is refused",
    "[scriptnet][cloud]")
{
  Host host;
  for (auto [baseUrl, namespaceId] :
       std::vector<std::pair<const char*, const char*>>{
           {"ftp://cloud.example.com", "ns_test"},
           {"https://cloud.example.com?x=1", "ns_test"},
           {"https://user@cloud.example.com", "ns_test"},
           {"https://", "ns_test"},
           {"https://cloud.example.com", "ns/../other"},
           {"https://cloud.example.com", ""}})
  {
    CloudEndpoint endpoint;
    endpoint.baseUrl = baseUrl;
    endpoint.namespaceId = namespaceId;
    std::string error;
    CHECK_FALSE(setCloudEndpoint(endpoint, &error));
    CHECK_FALSE(error.empty());
  }
  // Refused means off, not left on the endpoint before it.
  auto result = host.send("GET", "/v1/app/data/a");
  poll();
  CHECK(result->error.rfind("disabled:", 0) == 0);

  CloudEndpoint fine;
  fine.baseUrl = "https://cloud.example.com/";
  fine.namespaceId = "ns_Test-1";
  CHECK(setCloudEndpoint(fine));
}

TEST_CASE(
    "files on two endpoints keep their own sessions and tokens",
    "[scriptnet][cloud]")
{
  Host host;
  const std::string kTwo = "http://127.0.0.1:8701/v1/ns/ns_two";
  CloudEndpoint other;
  other.baseUrl = "http://127.0.0.1:8701";
  other.namespaceId = "ns_two";
  other.signHello = [](const std::string&, CloudEndpoint::HelloSigned done) {
    done(true, "{}", "S2");
  };
  CloudEndpointId two = addCloudEndpoint(std::move(other));
  REQUIRE(two != 0);

  auto mine = host.send("GET", "/v1/app/data/a");
  auto theirs = host.sendTo(two, "GET", "/v1/app/data/b");
  poll();
  host.answerAt(
      host.indexOf(std::string(kBase) + "/hello/challenge"),
      200,
      "{\"challenge\":\"v1.one\"}");
  host.answerAt(
      host.indexOf(kTwo + "/hello/challenge"),
      200,
      "{\"challenge\":\"v1.two\"}");
  size_t helloTwo = host.indexOf(kTwo + "/hello");
  CHECK(
      text(host.transport->starts[helloTwo].request)
          .find("\"challenge\":\"v1.two\"") != std::string::npos);
  CHECK(
      text(host.transport->starts[helloTwo].request).find("\"sig\":\"S2\"") !=
      std::string::npos);
  host.answerAt(
      host.indexOf(std::string(kBase) + "/hello"),
      200,
      "{\"token\":\"tok.ONE\"}");
  host.answerAt(helloTwo, 200, "{\"token\":\"tok.TWO\"}");

  size_t a = host.indexOf(std::string(kBase) + "/app/data/a");
  size_t b = host.indexOf(kTwo + "/app/data/b");
  CHECK(
      header(host.transport->starts[a].request, "authorization") ==
      "Bearer tok.ONE");
  CHECK(
      header(host.transport->starts[b].request, "authorization") ==
      "Bearer tok.TWO");
  host.answerAt(a, 200, "{}");
  host.answerAt(b, 200, "{}");
  CHECK(mine->status == 200);
  CHECK(theirs->status == 200);

  // Releasing the file's endpoint leaves the host's session alone, and the
  // file's later requests reach no endpoint at all.
  removeCloudEndpoint(two);
  auto gone = host.sendTo(two, "GET", "/v1/app/data/c");
  poll();
  CHECK(gone->error.rfind("disabled:", 0) == 0);
  auto still = host.send("GET", "/v1/app/data/d");
  size_t d = host.indexOf(std::string(kBase) + "/app/data/d");
  CHECK(
      header(host.transport->starts[d].request, "authorization") ==
      "Bearer tok.ONE");
  host.answerAt(d, 200, "{}");
  CHECK(still->status == 200);
}

TEST_CASE(
    "releasing an endpoint fails the requests waiting on its hello",
    "[scriptnet][cloud]")
{
  Host host;
  CloudEndpoint other;
  other.baseUrl = "http://127.0.0.1:8701";
  other.namespaceId = "ns_two";
  other.signHello = [](const std::string&, CloudEndpoint::HelloSigned done) {
    done(true, "{}", "S2");
  };
  CloudEndpointId two = addCloudEndpoint(std::move(other));
  auto waiting = host.sendTo(two, "GET", "/v1/app/data/a");
  poll();
  size_t challenge = host.indexOf(
      "http://127.0.0.1:8701/v1/ns/ns_two/hello/challenge");
  removeCloudEndpoint(two);
  poll();
  CHECK(waiting->error.rfind("aborted: the file", 0) == 0);
  // Its challenge's late answer starts nothing.
  size_t sent = host.transport->starts.size();
  host.answerAt(challenge, 200, "{\"challenge\":\"v1.two\"}");
  CHECK(host.transport->starts.size() == sent);
}

TEST_CASE("a refused file endpoint is added turned off", "[scriptnet][cloud]")
{
  Host host;
  CloudEndpoint bad;
  bad.baseUrl = "https://cloud.example.com";
  bad.namespaceId = "ns/../other";
  std::string error;
  CloudEndpointId id = addCloudEndpoint(std::move(bad), &error);
  CHECK(id != 0);
  CHECK_FALSE(error.empty());
  auto result = host.sendTo(id, "GET", "/v1/app/data/a");
  poll();
  CHECK(result->error.rfind("disabled:", 0) == 0);
  removeCloudEndpoint(id);
}

TEST_CASE(
    "allowedHosts governs plain fetches but not the host's endpoint",
    "[scriptnet][cloud]")
{
  Host host;
  setGeneralFetchEnabled(true);
  NetLimits limits;
  limits.allowedHosts = {"api.example.com"};
  setLimits(limits);

  auto cloud = host.send("GET", "/v1/players/me/data/save");
  poll();
  host.sayHello();
  REQUIRE(host.transport->starts.size() == 3);
  CHECK(host.sent().url == std::string(kBase) + "/players/me/data/save");
  host.answer(200, "{}");
  CHECK(cloud->status == 200);

  auto other = make_rcp<Result>();
  HttpRequest request;
  request.url = "https://other.example.com/";
  other->id = fetch(kOwner, std::move(request), other);
  poll();
  REQUIRE(other->settled);
  CHECK(other->error.rfind("policy: ", 0) == 0);
  CHECK(host.transport->starts.size() == 3);
}

TEST_CASE(
    "a hello answer past the size limit fails the requests that wait on it",
    "[scriptnet][cloud]")
{
  Host host;
  // Only a transport that reports in steps leaves the limit to scriptnet.
  if (!g_transportSteps)
  {
    return;
  }
  NetLimits limits;
  limits.maxResponseBodyBytes = 8;
  setLimits(limits);
  auto result = host.send("GET", "/v1/players/me/data/save");
  poll();
  host.answer(200, "{\"challenge\":\"v1.abc\",\"expires_in\":60}");
  REQUIRE(result->settled);
  CHECK(result->error.rfind("tooLarge: ", 0) == 0);
  // The session is free to say hello again for the next request.
  setLimits(NetLimits());
  auto next = host.send("GET", "/v1/players/me/data/save");
  poll();
  host.sayHello();
  host.answer(200, "{}");
  CHECK(next->status == 200);
}

TEST_CASE(
    "only requests to the host's endpoint are marked as its choice",
    "[scriptnet][cloud]")
{
  Host host;
  setGeneralFetchEnabled(true);
  auto data = host.send("GET", "/v1/app/data/a");
  poll();
  host.sayHello();
  // The challenge, the hello and the request itself.
  REQUIRE(host.transport->starts.size() == 3);
  for (const Transport::Start& start : host.transport->starts)
  {
    CHECK(start.request.hostChosen);
  }
  host.answer(200, "{}");
  CHECK(data->status == 200);

  // A script's own request never carries the mark, whatever it set.
  auto result = make_rcp<Result>();
  HttpRequest request;
  request.url = "https://example.com/";
  request.hostChosen = true;
  result->id = fetch(kOwner, std::move(request), result);
  poll();
  REQUIRE(host.transport->starts.size() == 4);
  CHECK_FALSE(host.sent().hostChosen);
}

#endif // WITH_RIVE_SCRIPTNET
