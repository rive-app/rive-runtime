#ifdef WITH_RIVE_SCRIPTNET

#include "catch.hpp"
#include "rive/scriptnet/net.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace rive;
using namespace rive::scriptnet;

namespace
{
class StreamProvider : public Provider
{
public:
  std::vector<RequestId> starts;
  std::vector<RequestId> aborts;
  std::vector<RequestId> resumes;

  void start(RequestId id, const HttpRequest&) override
  {
    starts.push_back(id);
  }
  void abort(RequestId id) override { aborts.push_back(id); }
  void resume(RequestId id) override { resumes.push_back(id); }
};

// Installs a provider and the given limits for one test.
struct StreamNetwork
{
  rcp<StreamProvider> provider = make_rcp<StreamProvider>();
  explicit StreamNetwork(const NetLimits& limits = NetLimits())
  {
    setProvider(provider);
    setLimits(limits);
  }
  ~StreamNetwork()
  {
    cancelAllForOwner(1);
    setProvider(nullptr);
    setLimits(NetLimits());
  }
};

class Listener : public FetchListener
{
public:
  explicit Listener(bool streams) : m_streams(streams) {}
  int responses = 0;
  int bodies = 0;
  bool cancelled = false;
  HttpResponse response;
  std::vector<NetError> errors;

  void onResponse(HttpResponse&& value) override
  {
    responses++;
    response = std::move(value);
  }
  void onError(const NetError& error) override { errors.push_back(error); }
  void onCancel() override { cancelled = true; }
  bool streams() const override { return m_streams; }
  void onBody() override { bodies++; }

private:
  bool m_streams;
};

HttpRequest getRequest(uint32_t timeoutMs = 0)
{
  HttpRequest request;
  request.url = "https://example.com/";
  request.timeoutMs = timeoutMs;
  return request;
}

HttpResponse head()
{
  HttpResponse response;
  response.status = 200;
  response.statusText = "OK";
  response.url = "https://example.com/";
  return response;
}

bool send(RequestId id, const std::string& text)
{
  return receive(id, (const uint8_t*)text.data(), text.size());
}

// What one readBody() answers, with the bytes as text.
struct Read
{
  BodyState state;
  std::string text;
  NetError error;
};

Read read(RequestId id)
{
  Read result;
  std::vector<uint8_t> bytes;
  result.state = readBody(id, &bytes, &result.error);
  result.text.assign(bytes.begin(), bytes.end());
  return result;
}
} // namespace

TEST_CASE(
    "a streamed response arrives at its headers and its body as it comes",
    "[scriptnet][stream]")
{
  StreamNetwork network;
  auto listener = make_rcp<Listener>(true);
  RequestId id = fetch(1, getRequest(), listener);
  respond(id, head());
  CHECK(listener->responses == 0);
  poll();
  REQUIRE(listener->responses == 1);
  CHECK(listener->response.status == 200);
  CHECK(listener->response.body.empty());

  CHECK(read(id).state == BodyState::Pending);
  CHECK(send(id, "hel"));
  CHECK(send(id, "lo"));
  CHECK(listener->bodies == 0);
  poll();
  // One announcement for all that arrived.
  CHECK(listener->bodies == 1);
  Read first = read(id);
  CHECK(first.state == BodyState::Data);
  CHECK(first.text == "hello");

  CHECK(read(id).state == BodyState::Pending);
  finish(id);
  poll();
  CHECK(listener->bodies == 2);
  CHECK(read(id).state == BodyState::End);
  // The request is forgotten once its end is read.
  Read gone = read(id);
  CHECK(gone.state == BodyState::Failed);
  CHECK(gone.error.code == NetErrorCode::Aborted);
  CHECK(!hasPendingWork());
  CHECK(listener->errors.empty());
}

TEST_CASE(
    "a transport that answers whole feeds a listener that streams",
    "[scriptnet][stream]")
{
  StreamNetwork network;
  auto listener = make_rcp<Listener>(true);
  RequestId id = fetch(1, getRequest(), listener);
  HttpResponse whole = head();
  whole.body = {'h', 'i'};
  complete(id, std::move(whole));
  poll();
  REQUIRE(listener->responses == 1);
  CHECK(listener->response.body.empty());
  Read body = read(id);
  CHECK(body.state == BodyState::Data);
  CHECK(body.text == "hi");
  CHECK(read(id).state == BodyState::End);
}

TEST_CASE(
    "a transport that streams feeds a listener that does not",
    "[scriptnet][stream]")
{
  StreamNetwork network;
  auto listener = make_rcp<Listener>(false);
  RequestId id = fetch(1, getRequest(), listener);
  respond(id, head());
  CHECK(send(id, "hel"));
  CHECK(send(id, "lo"));
  poll();
  // Nothing until the body is whole.
  CHECK(listener->responses == 0);
  finish(id);
  poll();
  REQUIRE(listener->responses == 1);
  CHECK(
      std::string(
          listener->response.body.begin(),
          listener->response.body.end()) == "hello");
  CHECK(!hasPendingWork());
}

TEST_CASE(
    "only a body that is not streamed is held to the size limit",
    "[scriptnet][stream]")
{
  NetLimits limits;
  limits.maxResponseBodyBytes = 4;
  StreamNetwork network(limits);

  auto whole = make_rcp<Listener>(false);
  RequestId refused = fetch(1, getRequest(), whole);
  respond(refused, head());
  CHECK(send(refused, "hel"));
  // Past the limit the transport is told to stop, and is aborted.
  CHECK(!send(refused, "lo"));
  CHECK(network.provider->aborts == std::vector<RequestId>{refused});
  poll();
  REQUIRE(whole->errors.size() == 1);
  CHECK(whole->errors[0].code == NetErrorCode::TooLarge);
  CHECK(whole->responses == 0);

  auto streamed = make_rcp<Listener>(true);
  RequestId id = fetch(1, getRequest(), streamed);
  respond(id, head());
  CHECK(send(id, "hello, world"));
  poll();
  CHECK(read(id).text == "hello, world");
  CHECK(streamed->errors.empty());
}

TEST_CASE(
    "a streamed body stops its transport while unread bytes fill its window",
    "[scriptnet][stream]")
{
  NetLimits limits;
  // Eight requests share 64 bytes: 8 for each body.
  limits.maxUndeliveredBytesPerOwner = 64;
  limits.maxInFlightPerOwner = 8;
  StreamNetwork network(limits);
  auto listener = make_rcp<Listener>(true);
  RequestId id = fetch(1, getRequest(), listener);
  respond(id, head());
  poll();
  CHECK(send(id, "12345"));
  // Bytes already read off the network are kept, and the transport waits.
  CHECK(!send(id, "67890"));
  CHECK(network.provider->resumes.empty());
  CHECK(read(id).text == "1234567890");
  CHECK(network.provider->resumes == std::vector<RequestId>{id});
  // A body that ended has nothing to resume.
  CHECK(!send(id, "abcdefgh"));
  finish(id);
  CHECK(read(id).text == "abcdefgh");
  CHECK(network.provider->resumes.size() == 1);
  CHECK(read(id).state == BodyState::End);
}

TEST_CASE(
    "a streamed body that fails part way gives what arrived, then the error",
    "[scriptnet][stream]")
{
  StreamNetwork network;
  auto listener = make_rcp<Listener>(true);
  RequestId id = fetch(1, getRequest(), listener);
  respond(id, head());
  poll();
  CHECK(send(id, "par"));
  fail(id, {NetErrorCode::Network, "the connection dropped"});
  CHECK(read(id).text == "par");
  Read failed = read(id);
  CHECK(failed.state == BodyState::Failed);
  CHECK(failed.error.code == NetErrorCode::Network);
  CHECK(failed.error.message == "the connection dropped");
  // A response's failure is never an error for the fetch that got it.
  poll();
  CHECK(listener->errors.empty());
}

TEST_CASE(
    "cancelling a streamed response aborts the body still arriving",
    "[scriptnet][stream]")
{
  StreamNetwork network;
  auto arriving = make_rcp<Listener>(true);
  RequestId first = fetch(1, getRequest(), arriving);
  respond(first, head());
  poll();
  CHECK(cancel(first));
  CHECK(arriving->cancelled);
  CHECK(network.provider->aborts == std::vector<RequestId>{first});

  // One the transport finished has nothing left to abort.
  auto ended = make_rcp<Listener>(true);
  RequestId second = fetch(1, getRequest(), ended);
  respond(second, head());
  finish(second);
  poll();
  CHECK(cancel(second));
  CHECK(network.provider->aborts.size() == 1);
}

TEST_CASE(
    "a streamed body keeps its request's slot until it ends",
    "[scriptnet][stream]")
{
  NetLimits limits;
  limits.maxInFlightPerOwner = 1;
  StreamNetwork network(limits);
  auto first = make_rcp<Listener>(true);
  auto second = make_rcp<Listener>(true);
  RequestId id = fetch(1, getRequest(), first);
  fetch(1, getRequest(), second);
  respond(id, head());
  poll();
  // Its headers are in, but the transport is still busy with its body.
  CHECK(network.provider->starts.size() == 1);
  finish(id);
  poll();
  CHECK(network.provider->starts.size() == 2);
}

TEST_CASE(
    "the timeout ends at the headers of a streamed response",
    "[scriptnet][stream]")
{
  NetLimits limits;
  limits.watchdogGraceMs = 0;
  StreamNetwork network(limits);
  auto streamed = make_rcp<Listener>(true);
  auto whole = make_rcp<Listener>(false);
  RequestId streamedId = fetch(1, getRequest(1), streamed);
  RequestId wholeId = fetch(1, getRequest(1), whole);
  // The transport hears which body its own timer still covers.
  CHECK_FALSE(respond(streamedId, head()));
  CHECK(respond(wholeId, head()));
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  poll();
  CHECK(streamed->responses == 1);
  CHECK(streamed->errors.empty());
  CHECK(read(streamedId).state == BodyState::Pending);
  // One read whole is timed to its last byte.
  REQUIRE(whole->errors.size() == 1);
  CHECK(whole->errors[0].code == NetErrorCode::Timeout);
}

TEST_CASE(
    "a streamed body nobody waits on is not pending work",
    "[scriptnet][stream]")
{
  StreamNetwork network;
  auto listener = make_rcp<Listener>(true);
  RequestId id = fetch(1, getRequest(), listener);
  CHECK(hasPendingWork());
  respond(id, head());
  CHECK(hasPendingWork());
  poll();
  CHECK(!hasPendingWork());
  CHECK(!hasPendingWorkForOwner(1));
  CHECK(read(id).state == BodyState::Pending);
  CHECK(hasPendingWork());
  CHECK(send(id, "x"));
  CHECK(hasPendingWork());
  poll();
  CHECK(!hasPendingWork());
}

TEST_CASE(
    "cancelling a delivered response frees its slot and its unread bytes",
    "[scriptnet][stream]")
{
  NetLimits limits;
  // One request at a time, and two unread bytes hold the next one back.
  limits.maxInFlightPerOwner = 1;
  limits.maxUndeliveredBytesPerOwner = 2;
  StreamNetwork network(limits);
  auto listener = make_rcp<Listener>(true);
  RequestId id = fetch(1, getRequest(), listener);
  respond(id, head());
  CHECK(!send(id, "ab"));
  auto next = make_rcp<Listener>(true);
  fetch(1, getRequest(), next);
  CHECK(network.provider->starts.size() == 1);
  // Nothing has the first response yet, so no collector could free it.
  CHECK(!waitsOnUnreadBodies(1));
  poll();
  CHECK(listener->responses == 1);
  CHECK(waitsOnUnreadBodies(1));
  // A body something waits to read is in use, which no collector frees.
  CHECK(read(id).text == "ab");
  CHECK(read(id).state == BodyState::Pending);
  CHECK(!waitsOnUnreadBodies(1));
  CHECK(!send(id, "cd"));
  CHECK(!waitsOnUnreadBodies(1));
  poll();
  CHECK(waitsOnUnreadBodies(1));

  CHECK(cancel(id));
  CHECK(listener->cancelled);
  CHECK(network.provider->aborts == std::vector<RequestId>{id});
  // Its slot and its unread bytes went with it, so the next one starts.
  poll();
  CHECK(network.provider->starts.size() == 2);
  CHECK(!waitsOnUnreadBodies(1));
}

TEST_CASE(
    "a response cancelled with its head still queued is dropped",
    "[scriptnet][stream]")
{
  StreamNetwork network;
  auto listener = make_rcp<Listener>(true);
  RequestId id = fetch(1, getRequest(), listener);
  respond(id, head());
  CHECK(send(id, "ab"));
  CHECK(cancel(id));
  poll();
  CHECK(listener->responses == 0);
  CHECK(listener->cancelled);
  CHECK(network.provider->aborts == std::vector<RequestId>{id});
  CHECK(!hasPendingWork());
}

TEST_CASE(
    "a transport that says a response twice hears the same answer",
    "[scriptnet][stream]")
{
  StreamNetwork network;
  auto streamed = make_rcp<Listener>(true);
  auto whole = make_rcp<Listener>(false);
  RequestId streamedId = fetch(1, getRequest(), streamed);
  RequestId wholeId = fetch(1, getRequest(), whole);
  CHECK_FALSE(respond(streamedId, head()));
  CHECK_FALSE(respond(streamedId, head()));
  CHECK(respond(wholeId, head()));
  CHECK(respond(wholeId, head()));
  poll();
  CHECK(streamed->responses == 1);
}

TEST_CASE(
    "a whole answer larger than a body's window is read in one piece",
    "[scriptnet][stream]")
{
  NetLimits limits;
  limits.maxUndeliveredBytesPerOwner = 64;
  limits.maxInFlightPerOwner = 8;
  StreamNetwork network(limits);
  auto listener = make_rcp<Listener>(true);
  RequestId id = fetch(1, getRequest(), listener);
  HttpResponse whole = head();
  whole.body.assign(20, 'x');
  complete(id, std::move(whole));
  poll();
  CHECK(read(id).text == std::string(20, 'x'));
  CHECK(read(id).state == BodyState::End);
  // A transport that is done has nothing to resume.
  CHECK(network.provider->resumes.empty());
}

TEST_CASE(
    "an empty run never stops a body, however small its window",
    "[scriptnet][stream]")
{
  NetLimits limits;
  // Less than a byte for each of the eight requests.
  limits.maxUndeliveredBytesPerOwner = 4;
  limits.maxInFlightPerOwner = 8;
  StreamNetwork network(limits);
  auto listener = make_rcp<Listener>(true);
  RequestId id = fetch(1, getRequest(), listener);
  respond(id, head());
  poll();
  CHECK(send(id, ""));
  CHECK(read(id).state == BodyState::Pending);
  // One byte fills it, and reading that byte opens it again.
  CHECK(!send(id, "x"));
  CHECK(read(id).text == "x");
  CHECK(network.provider->resumes == std::vector<RequestId>{id});
}

TEST_CASE(
    "a body that never arrives does not time out once its headers have",
    "[scriptnet][stream]")
{
  NetLimits limits;
  limits.watchdogGraceMs = 0;
  StreamNetwork network(limits);
  auto waiting = make_rcp<Listener>(true);
  auto answered = make_rcp<Listener>(true);
  // One never answers at all, and times out as any request does.
  fetch(1, getRequest(1), waiting);
  RequestId id = fetch(1, getRequest(1), answered);
  respond(id, head());
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  poll();
  REQUIRE(waiting->errors.size() == 1);
  CHECK(waiting->errors[0].code == NetErrorCode::Timeout);
  CHECK(answered->errors.empty());
  CHECK(read(id).state == BodyState::Pending);
}

TEST_CASE(
    "a read that is given up is no longer pending work",
    "[scriptnet][stream]")
{
  StreamNetwork network;
  auto listener = make_rcp<Listener>(true);
  RequestId id = fetch(1, getRequest(), listener);
  respond(id, head());
  poll();
  CHECK(read(id).state == BodyState::Pending);
  CHECK(hasPendingWork());
  forgetRead(id);
  CHECK(!hasPendingWork());
  // What arrives is not announced, and waits for the next read.
  CHECK(send(id, "x"));
  poll();
  CHECK(listener->bodies == 0);
  CHECK(read(id).text == "x");
}

#endif
