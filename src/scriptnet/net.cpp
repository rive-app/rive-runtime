/*
 * Copyright 2026 Rive
 */

#ifdef WITH_RIVE_SCRIPTNET

#include "rive/scriptnet/net.hpp"

#include <algorithm>
#include <cctype>

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace rive::scriptnet
{

const char* netErrorCodeName(NetErrorCode code)
{
  switch (code)
  {
    case NetErrorCode::Disabled:
      return "disabled";
    case NetErrorCode::Policy:
      return "policy";
    case NetErrorCode::RateLimited:
      return "rateLimited";
    case NetErrorCode::Timeout:
      return "timeout";
    case NetErrorCode::Aborted:
      return "aborted";
    case NetErrorCode::TooLarge:
      return "tooLarge";
    case NetErrorCode::Network:
      return "network";
  }
  return "network";
}

NetErrorCode netErrorCodeFromName(const char* name, size_t length)
{
  std::string value(name, length);
  for (NetErrorCode code :
       {NetErrorCode::Disabled,
        NetErrorCode::Policy,
        NetErrorCode::RateLimited,
        NetErrorCode::Timeout,
        NetErrorCode::Aborted,
        NetErrorCode::TooLarge})
  {
    if (value == netErrorCodeName(code))
    {
      return code;
    }
  }
  return NetErrorCode::Network;
}

const char* standardReasonPhrase(uint16_t status)
{
  switch (status)
  {
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 202:
      return "Accepted";
    case 204:
      return "No Content";
    case 206:
      return "Partial Content";
    case 301:
      return "Moved Permanently";
    case 302:
      return "Found";
    case 303:
      return "See Other";
    case 304:
      return "Not Modified";
    case 307:
      return "Temporary Redirect";
    case 308:
      return "Permanent Redirect";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 409:
      return "Conflict";
    case 410:
      return "Gone";
    case 413:
      return "Content Too Large";
    case 415:
      return "Unsupported Media Type";
    case 422:
      return "Unprocessable Content";
    case 429:
      return "Too Many Requests";
    case 500:
      return "Internal Server Error";
    case 501:
      return "Not Implemented";
    case 502:
      return "Bad Gateway";
    case 503:
      return "Service Unavailable";
    case 504:
      return "Gateway Timeout";
  }
  return "";
}

#if !defined(__APPLE__) && !defined(_WIN32) && !defined(__ANDROID__) &&        \
    !defined(WITH_RIVE_SCRIPTNET_CURL)
// Apple, Windows, Android and builds with our curl have transports. Hosts
// elsewhere keep denying until theirs lands.
rcp<Provider> makePlatformProvider() { return nullptr; }
#endif

namespace
{
using Clock = std::chrono::steady_clock;

enum class HelloStep : uint8_t
{
  None,
  Challenge,
  Hello,
};

// No script owns the host's own hello calls.
constexpr uint64_t kHostOwner = UINT64_MAX;

// How often a 401 with no challenge may cost a new hello.
constexpr std::chrono::minutes kBlindHelloInterval{1};

// What a request not yet sent fails with once the host names another endpoint.
NetError endpointReplaced()
{
  return {NetErrorCode::Aborted, "the host named another Rive Cloud endpoint"};
}

// The top-level members of a JSON object, each as the raw text of its name
// (quotes included) and of its value. False for anything else. It checks
// only as much as it needs to split the text, since the engine wrote it.
bool jsonMembers(
    const std::string& text,
    std::vector<std::pair<std::string, std::string>>* members)
{
  size_t at = 0;
  auto skipSpace = [&]() {
    while (at < text.size() && (text[at] == ' ' || text[at] == '\t' ||
                                text[at] == '\r' || text[at] == '\n'))
    {
      at++;
    }
  };
  // Leaves `at` past the closing quote.
  auto skipString = [&]() {
    if (at >= text.size() || text[at] != '"')
    {
      return false;
    }
    for (at++; at < text.size(); at++)
    {
      if (text[at] == '\\')
      {
        at++;
      }
      else if (text[at] == '"')
      {
        at++;
        return true;
      }
    }
    return false;
  };
  skipSpace();
  if (at >= text.size() || text[at] != '{')
  {
    return false;
  }
  at++;
  skipSpace();
  if (at < text.size() && text[at] == '}')
  {
    return true;
  }
  for (;;)
  {
    skipSpace();
    size_t nameStart = at;
    if (!skipString())
    {
      return false;
    }
    std::string name = text.substr(nameStart, at - nameStart);
    skipSpace();
    if (at >= text.size() || text[at] != ':')
    {
      return false;
    }
    at++;
    skipSpace();
    size_t valueStart = at;
    if (at < text.size() && text[at] == '"')
    {
      if (!skipString())
      {
        return false;
      }
    }
    else
    {
      int depth = 0;
      while (at < text.size())
      {
        char c = text[at];
        if (c == '"')
        {
          if (!skipString())
          {
            return false;
          }
          continue;
        }
        if (c == '{' || c == '[')
        {
          depth++;
        }
        else if (c == '}' || c == ']')
        {
          if (depth == 0)
          {
            break;
          }
          depth--;
        }
        else if (c == ',' && depth == 0)
        {
          break;
        }
        at++;
      }
    }
    size_t valueEnd = at;
    while (valueEnd > valueStart &&
           (text[valueEnd - 1] == ' ' || text[valueEnd - 1] == '\t' ||
            text[valueEnd - 1] == '\r' || text[valueEnd - 1] == '\n'))
    {
      valueEnd--;
    }
    if (valueEnd == valueStart)
    {
      return false;
    }
    members->push_back(
        {std::move(name), text.substr(valueStart, valueEnd - valueStart)});
    skipSpace();
    if (at >= text.size())
    {
      return false;
    }
    if (text[at] == '}')
    {
      return true;
    }
    if (text[at] != ',')
    {
      return false;
    }
    at++;
  }
}

// The raw text of the member called `name`, or "" without one.
std::string jsonMember(
    const std::vector<std::pair<std::string, std::string>>& members,
    const char* name)
{
  std::string quoted = std::string("\"") + name + "\"";
  for (const auto& member : members)
  {
    if (member.first == quoted)
    {
      return member.second;
    }
  }
  return "";
}

// What a raw JSON string holds when it is plain: no escapes, and only the
// characters tokens, challenges and player IDs are made of. "" otherwise.
std::string plainString(const std::string& raw)
{
  if (raw.size() < 2 || raw.front() != '"' || raw.back() != '"')
  {
    return "";
  }
  for (size_t i = 1; i + 1 < raw.size(); i++)
  {
    char c = raw[i];
    if (!std::isalnum((unsigned char)c) && c != '.' && c != '-' && c != '_')
    {
      return "";
    }
  }
  return raw.substr(1, raw.size() - 2);
}

std::string bodyText(const std::vector<uint8_t>& body)
{
  return std::string(body.begin(), body.end());
}

// Whether a segment of the path is "." or "..", which a transport may resolve
// and which a URL parser also takes spelled %2e. A key may hold dots.
bool hasDotSegment(const std::string& path)
{
  size_t end = std::min(path.find('?'), path.size());
  for (size_t start = 0; start <= end;)
  {
    size_t stop = std::min(path.find('/', start), end);
    size_t at = start;
    size_t dots = 0;
    for (; at < stop; dots++)
    {
      if (path[at] == '.')
      {
        at++;
      }
      else if (
          stop - at >= 3 && path[at] == '%' && path[at + 1] == '2' &&
          (path[at + 2] == 'e' || path[at + 2] == 'E'))
      {
        at += 3;
      }
      else
      {
        break;
      }
    }
    if (at == stop && (dots == 1 || dots == 2))
    {
      return true;
    }
    start = stop + 1;
  }
  return false;
}

// The player this host is, to the Rive Cloud engine. The host says hello,
// keeps the token and adds it to each rive-cloud: request, so a script never
// holds it.
struct CloudSession
{
  std::string token;
  uint64_t tokenGeneration = 0;
  // What a script's POST /v1/hello is answered with: the engine's hello
  // answer without its token.
  std::string answer;
  // The player's name as the engine last gave it, and one a script asked
  // for that the next hello carries. Both as raw JSON strings.
  std::string displayName;
  std::string wantedName;
  // The name the hello now out carries, to tell whether one asked for
  // meanwhile still has to be sent.
  std::string helloName;
  // A challenge to sign: from hello/challenge, or from a 401's body.
  std::string challenge;
  // A hello call is out, or its signature is awaited, so no other starts.
  bool helloInFlight = false;
  // Nonzero while the host's key signs; a signature that comes back under
  // another ticket, after the hello failed or the endpoint changed, is dropped.
  uint64_t signTicket = 0;
  Clock::time_point signDeadline;
  // The engine refused a signed challenge and gave another, which is signed
  // once more and no further.
  bool signedAgain = false;
  // The earliest a 401 with no challenge may next drop the token.
  Clock::time_point nextBlindHello;
  // Rises when the host names another endpoint, so an answer from the last
  // one is dropped.
  uint64_t epoch = 0;
  // Put before each Idempotency-Key a script sends, so its keys, which count
  // from 1 in every run, never match another run's.
  std::string keyPrefix;
};

// An endpoint and the session the host keeps with it. Endpoint 0 is the one
// setCloudEndpoint names for the whole host; files may add their own.
struct CloudState
{
  CloudEndpoint endpoint;
  CloudSession session;
};

struct Outstanding
{
  uint64_t ownerId = 0;
  // The thread that called fetch(), and the only one poll() delivers on.
  std::thread::id thread;
  rcp<FetchListener> listener;
  Clock::time_point deadline;
  // Waiting in its owner's queue for a slot; the request is kept here
  // until it starts.
  bool queued = false;
  HttpRequest request;
  // Handed to the provider, as opposed to refused up front or queued.
  bool started = false;
  // Holding one of its owner's in-flight slots, from its start until the
  // transport answers, or has finished a body that streams.
  bool counted = false;
  // Answered and counted against its owner's undelivered budget until
  // poll() delivers or drops it. undeliveredBytes is its body's bytes, or
  // the unread bytes of a body that streams.
  bool undelivered = false;
  size_t undeliveredBytes = 0;
  // An outcome is queued, or the head of a response that streams was; any
  // later one is dropped.
  bool settled = false;
  // The transport that started it, and so the one to abort it, even after
  // setProvider() swaps transports.
  rcp<Provider> provider;
  // Provider::start() is running without the lock, so the transport may
  // not know the request yet: a cancel meanwhile only marks it, and
  // fetch() aborts once start() has returned.
  bool starting = false;
  bool cancelled = false;
  // A rive-cloud: request. It waits for the host's player token, is sent
  // with it, and is kept here so it can be sent again.
  bool cloud = false;
  // A script's POST /v1/hello. The host answers it from what the engine's
  // hello said, without the token, and never sends it.
  bool session = false;
  // Sent again after a 401, with a new token. Once.
  bool retried = false;
  // Which token it was sent with, so one expired token is dropped once
  // however many requests come back 401 with it.
  uint64_t tokenGeneration = 0;
  // One of the host's own two hello calls. It has no listener and no owner,
  // and its answer changes the session instead of reaching a script.
  HelloStep helloStep = HelloStep::None;
  // The endpoint a hello call or a rive-cloud: request was made for, and its
  // session then: a session is replaced, or its endpoint removed, under it.
  CloudEndpointId cloudId = 0;
  uint64_t epoch = 0;
  // The rive-cloud: URL the script asked for, which is the URL its response
  // names: the engine's address and the namespace are the host's to know.
  std::string scriptUrl;
  // Its listener reads the body with readBody().
  bool listenerStreams = false;
  // The body goes to that listener as the transport reports it; a
  // rive-cloud: answer only once the host has read it whole.
  bool streams = false;
  // respond() gave the status and headers. Unless the body streams,
  // `response` holds them and what has arrived of it.
  bool responded = false;
  HttpResponse response;
  // The rest is for a body that streams. Its bytes not yet read, counted in
  // undeliveredBytes.
  std::deque<std::vector<uint8_t>> chunks;
  // poll() delivered the response, so readBody() may be called.
  bool delivered = false;
  // The transport has finished the body, or failed it with bodyError.
  bool ended = false;
  bool failed = false;
  NetError bodyError;
  // readBody() answered Pending, so the next change is announced.
  bool wantsBody = false;
  // An onBody() is queued.
  bool bodyQueued = false;
  // receive() answered false, so the transport waits for resume().
  bool paused = false;
};

struct Owner
{
  // Requests the transport has yet to answer, for the in-flight limit.
  uint32_t inFlight = 0;
  // Requests waiting for a slot, oldest first, and their bodies' bytes.
  std::deque<RequestId> waiting;
  size_t waitingBodyBytes = 0;
  // Answers poll() has yet to deliver, and their bodies' bytes.
  uint32_t undelivered = 0;
  size_t undeliveredBytes = 0;
};

struct Outcome
{
  RequestId id = 0;
  uint64_t sequence = 0;
  bool ok = false;
  // An onBody() rather than a response or an error.
  bool body = false;
  HttpResponse response;
  NetError error;
};

class Net
{
public:
  static Net& instance()
  {
    // Never destroyed: transport threads may still report during exit.
    static Net& net = *new Net();
    return net;
  }

  // Everything below is guarded by mutex. Listeners and providers are only
  // ever called with it released, since both can re-enter this file.
  std::mutex mutex;
  rcp<Provider> provider;
  // By id; 0, the host's, is always here, off until the host names it.
  std::unordered_map<CloudEndpointId, CloudState> clouds{{0, CloudState()}};
  CloudEndpointId lastCloudId = 0;
  bool generalFetchEnabled = true;
  NetLimits limits;
  RequestId nextId = 1;
  std::unordered_map<RequestId, Outstanding> outstanding;
  // Outcomes whose request is no longer outstanding, or was cancelled while
  // starting, are dropped when poll() reaches them.
  std::deque<Outcome> outcomes;
  uint64_t nextSequence = 0;
  // Outlives each session, so a ticket names one signature for good.
  uint64_t lastSignTicket = 0;
  std::unordered_map<uint64_t, Owner> owners;
  // outstanding.size(), readable without the lock so idle polls stay cheap.
  std::atomic<size_t> outstandingCount{0};

  CloudState* cloudOf(CloudEndpointId id)
  {
    auto it = clouds.find(id);
    return it == clouds.end() ? nullptr : &it->second;
  }

  // Whether the session `entry` was made for is still the one its endpoint
  // keeps.
  bool isCurrent(const Outstanding& entry)
  {
    CloudState* state = cloudOf(entry.cloudId);
    return state != nullptr && state->session.epoch == entry.epoch;
  }

  // Forgets the request. Unless poll() just delivered its outcome, any
  // outcome still queued goes too, so a cancelled answer keeps no memory.
  void erase(
      std::unordered_map<RequestId, Outstanding>::iterator it,
      bool delivered = false)
  {
    if (it->second.settled && !delivered)
    {
      for (auto at = outcomes.begin(); at != outcomes.end(); ++at)
      {
        if (at->id == it->first)
        {
          outcomes.erase(at);
          break;
        }
      }
    }
    release(it->second);
    auto owner = owners.find(it->second.ownerId);
    if (owner != owners.end())
    {
      owner->second.undelivered -= it->second.undelivered ? 1 : 0;
      owner->second.undeliveredBytes -= it->second.undeliveredBytes;
    }
    if (it->second.queued)
    {
      if (owner != owners.end())
      {
        std::deque<RequestId>& waiting = owner->second.waiting;
        for (auto at = waiting.begin(); at != waiting.end(); ++at)
        {
          if (*at == it->first)
          {
            waiting.erase(at);
            break;
          }
        }
        owner->second.waitingBodyBytes -= it->second.request.body.size();
      }
    }
    outstanding.erase(it);
    outstandingCount.store(outstanding.size(), std::memory_order_relaxed);
  }

  // The transport answered: its slot frees for the next request, and the
  // answer holds part of the undelivered budget until poll() delivers it.
  void answer(Outstanding& entry, size_t bytes)
  {
    if (!entry.counted)
    {
      return;
    }
    release(entry);
    hold(entry, bytes);
  }

  // Counts an answer, or the head of one whose body still arrives, against
  // its owner's undelivered budget.
  void hold(Outstanding& entry, size_t bytes)
  {
    auto owner = owners.find(entry.ownerId);
    if (owner != owners.end())
    {
      owner->second.undelivered++;
      owner->second.undeliveredBytes += bytes;
      entry.undelivered = true;
      entry.undeliveredBytes = bytes;
    }
  }

  // Whether the transport still works on the entry, so a cancel aborts it.
  static bool withTransport(const Outstanding& entry)
  {
    return entry.started &&
           (entry.streams && entry.responded ? !entry.ended : !entry.settled);
  }

  // The unread bytes one streamed body may hold before its transport waits:
  // an equal share of the owner's budget for each request it may have out.
  size_t bodyWindow() const
  {
    // At least a byte, so an empty run never stops a transport.
    return std::max<size_t>(
        1,
        limits.maxUndeliveredBytesPerOwner /
            std::max<uint32_t>(1, limits.maxInFlightPerOwner));
  }

  // Announces a change to a streamed body its listener waits on.
  void announceBody(RequestId id, Outstanding& entry)
  {
    if (!entry.wantsBody)
    {
      return;
    }
    entry.wantsBody = false;
    // One announcement in the queue is enough.
    if (entry.bodyQueued)
    {
      return;
    }
    entry.bodyQueued = true;
    Outcome outcome;
    outcome.id = id;
    outcome.body = true;
    queue(std::move(outcome));
  }

  // Whether poll() still owes the entry's listener a call.
  static bool owesDelivery(const Outstanding& entry)
  {
    return !entry.delivered || entry.wantsBody || entry.bodyQueued;
  }

  // The status and headers of a response that streams, queued for poll()
  // while its body keeps the transport.
  void respondStream(RequestId id, Outstanding& entry, HttpResponse&& head)
  {
    entry.settled = true;
    entry.responded = true;
    if (entry.counted)
    {
      hold(entry, 0);
    }
    Outcome outcome;
    outcome.id = id;
    outcome.ok = true;
    outcome.response = std::move(head);
    queue(std::move(outcome));
  }

  // More of a streamed body; false once its unread bytes fill their window.
  bool receiveStream(
      RequestId id,
      Outstanding& entry,
      std::vector<uint8_t>&& chunk)
  {
    auto owner = owners.find(entry.ownerId);
    if (!chunk.empty() && owner != owners.end())
    {
      owner->second.undeliveredBytes += chunk.size();
      entry.undeliveredBytes += chunk.size();
    }
    if (!chunk.empty())
    {
      entry.chunks.push_back(std::move(chunk));
      announceBody(id, entry);
    }
    // Kept until a read takes the bytes, whatever the limits become.
    entry.paused = entry.paused || entry.undeliveredBytes >= bodyWindow();
    return !entry.paused;
  }

  // The transport is done with a streamed body, having failed it when
  // `error` is given.
  void endStream(RequestId id, Outstanding& entry, const NetError* error)
  {
    entry.ended = true;
    if (error != nullptr)
    {
      entry.failed = true;
      entry.bodyError = *error;
    }
    release(entry);
    announceBody(id, entry);
  }

  // A whole response for poll(): as it is, or for a listener that streams
  // as its head with the body there to read.
  void deliverWhole(RequestId id, Outstanding& entry, HttpResponse&& response)
  {
    if (!entry.listenerStreams)
    {
      entry.settled = true;
      answer(entry, response.body.size());
      Outcome outcome;
      outcome.id = id;
      outcome.ok = true;
      outcome.response = std::move(response);
      queue(std::move(outcome));
      return;
    }
    std::vector<uint8_t> body = std::move(response.body);
    response.body.clear();
    entry.streams = true;
    respondStream(id, entry, std::move(response));
    receiveStream(id, entry, std::move(body));
    endStream(id, entry, nullptr);
  }

  // Frees the entry's in-flight slot, once.
  void release(Outstanding& entry)
  {
    if (!entry.counted)
    {
      return;
    }
    entry.counted = false;
    auto owner = owners.find(entry.ownerId);
    if (owner != owners.end() && owner->second.inFlight > 0)
    {
      owner->second.inFlight--;
    }
  }

  NetError tooLarge() const
  {
    return {
        NetErrorCode::TooLarge,
        "the response body is larger than " +
            std::to_string(limits.maxResponseBodyBytes) + " bytes"};
  }

  RequestId allocateId()
  {
    RequestId id = nextId++;
    while (id == 0 || outstanding.count(id) != 0)
    {
      id = nextId++;
    }
    return id;
  }

  void queue(Outcome&& outcome)
  {
    outcome.sequence = nextSequence++;
    outcomes.push_back(std::move(outcome));
  }

  void settle(RequestId id, Outstanding& entry, NetError&& error)
  {
    entry.settled = true;
    answer(entry, 0);
    Outcome outcome;
    outcome.id = id;
    outcome.error = std::move(error);
    queue(std::move(outcome));
  }

  // A request that is not streaming failed: the host's own hello fails the
  // session, and a script's request settles with the error.
  void failWhole(
      std::unordered_map<RequestId, Outstanding>::iterator it,
      NetError&& error)
  {
    if (it->second.helloStep == HelloStep::None)
    {
      settle(it->first, it->second, std::move(error));
      return;
    }
    bool current = isCurrent(it->second);
    CloudEndpointId cloudId = it->second.cloudId;
    erase(it);
    if (current)
    {
      failHello(cloudId, nullptr, &error);
    }
  }

  // Detaches the first deliverable outcome for `thread` queued before
  // `before`.
  bool take(
      std::thread::id thread,
      uint64_t before,
      rcp<FetchListener>* listener,
      Outcome* outcome)
  {
    for (auto it = outcomes.begin();
         it != outcomes.end() && it->sequence < before;)
    {
      auto entry = outstanding.find(it->id);
      if (entry == outstanding.end() || entry->second.cancelled)
      {
        it = outcomes.erase(it);
        continue;
      }
      if (entry->second.thread != thread)
      {
        ++it;
        continue;
      }
      *outcome = std::move(*it);
      outcomes.erase(it);
      Outstanding& found = entry->second;
      if (outcome->body)
      {
        found.bodyQueued = false;
        *listener = found.listener;
        return true;
      }
      if (outcome->ok && found.streams && found.responded)
      {
        // Kept for its body; only the head leaves the undelivered count.
        found.delivered = true;
        auto owner = owners.find(found.ownerId);
        if (found.undelivered && owner != owners.end())
        {
          owner->second.undelivered--;
        }
        found.undelivered = false;
        *listener = found.listener;
        return true;
      }
      *listener = std::move(found.listener);
      erase(entry, true);
      return true;
    }
    return false;
  }

  // The host and the policy; fills `error` and returns false to refuse.
  bool allow(HttpRequest& request, Outstanding& entry, NetError* error)
  {
    if (provider == nullptr)
    {
      error->code = NetErrorCode::Disabled;
      error->message = "network access is not enabled for scripts in this host";
      return false;
    }
    static const std::string kCloudScheme = "rive-cloud:";
    if (request.url.compare(0, kCloudScheme.size(), kCloudScheme) == 0)
    {
      return allowCloud(request, entry, kCloudScheme.size(), error);
    }
    if (!generalFetchEnabled)
    {
      error->code = NetErrorCode::Disabled;
      error->message = "network access is not enabled for scripts in this host";
      return false;
    }
    return NetPolicy::check(request, limits, error);
  }

  // A rive-cloud: request passes every rule a fetch does (method, headers,
  // sizes, URL characters), judged against a stand-in public address. Then the
  // host's own address replaces it, so http: and loopback are reachable here
  // and only here, and only at the one address the host named.
  bool allowCloud(
      HttpRequest& request,
      Outstanding& entry,
      size_t schemeLength,
      NetError* error)
  {
    CloudState* state = cloudOf(entry.cloudId);
    if (state == nullptr || state->endpoint.baseUrl.empty())
    {
      error->code = NetErrorCode::Disabled;
      error->message = "this host has no Rive Cloud endpoint";
      return false;
    }
    const CloudEndpoint& cloud = state->endpoint;
    CloudSession& session = state->session;
    std::string path = request.url.substr(schemeLength);
    // A fragment never reaches the engine, so /v1/hello#x would be its hello
    // under a name the hello rule below does not know.
    if (path.compare(0, 4, "/v1/") != 0 || hasDotSegment(path) ||
        path.find('#') != std::string::npos)
    {
      error->code = NetErrorCode::Policy;
      error->message = "a rive-cloud: URL is a path under /v1/ with no . or "
                       ".. segment and no fragment";
      return false;
    }
    // The engine decodes a segment before it matches a route, so an encoded
    // route family would slip past the hello rule below. None needs encoding.
    size_t familyEnd = path.find_first_of("/?", 4);
    if (path.substr(
                4,
                familyEnd == std::string::npos ? familyEnd : familyEnd - 4)
            .find('%') != std::string::npos)
    {
      error->code = NetErrorCode::Policy;
      error->message = "a rive-cloud: route is not percent-encoded";
      return false;
    }
    HttpRequest probe = request;
    probe.url = "https://cloud.example.com" + path;
    // The host chose the endpoint, so the scripts' allowedHosts don't govern
    // it.
    NetLimits probeLimits = limits;
    probeLimits.allowedHosts = {"*"};
    if (!NetPolicy::check(probe, probeLimits, error))
    {
      return false;
    }
    // Keep what the check normalized, such as the default timeout.
    request = std::move(probe);
    // The namespace goes in the path, where a CORS preflight can see it.
    request.url = cloud.baseUrl + "/v1/ns/" + cloud.namespaceId +
                  path.substr(3);
    request.hostChosen = true;
    // The host's values win over anything the script set, and the token is
    // the host's to add.
    for (size_t i = request.headers.size(); i-- > 0;)
    {
      std::string lower = lowerAscii(request.headers[i].name);
      if (lower == "x-rive-namespace" || lower == "x-rive-install-id" ||
          lower == "x-rive-challenge" || lower == "authorization")
      {
        request.headers.erase(request.headers.begin() + i);
      }
      else if (lower == "idempotency-key")
      {
        // The owner is one VM, so a reloaded script, which counts its keys
        // from 1 again, never repeats the last one's.
        request.headers[i].value = session.keyPrefix + "." +
                                   std::to_string(entry.ownerId) + "." +
                                   request.headers[i].value;
      }
    }
    entry.cloud = true;
    entry.epoch = session.epoch;
    entry.scriptUrl = "rive-cloud:" + path;
    std::string route = path.substr(0, path.find('?'));
    if (route.compare(0, 9, "/v1/hello") != 0 ||
        (route.size() > 9 && route[9] != '/'))
    {
      return true;
    }
    // The host says hello. A script's own POST /v1/hello asks who the player
    // is, and may name them; nothing else under hello is a script's to call.
    if (route != "/v1/hello" || request.method != "POST")
    {
      error->code = NetErrorCode::Policy;
      error->message = "the host says hello; a script only posts /v1/hello";
      return false;
    }
    entry.session = true;
    std::vector<std::pair<std::string, std::string>> members;
    std::string body = bodyText(request.body);
    if (body.find_first_not_of(" \t\r\n") != std::string::npos &&
        !jsonMembers(body, &members))
    {
      error->code = NetErrorCode::Policy;
      error->message = "a hello body is a JSON object";
      return false;
    }
    std::string name = jsonMember(members, "display_name");
    if (name.size() >= 2 && name.front() == '"' && name != session.displayName)
    {
      // Only a hello carries a name to the engine, so the next request
      // waits for a new one.
      session.wantedName = name;
      session.token.clear();
    }
    return true;
  }

  // What a script's POST /v1/hello resolves with.
  HttpResponse sessionAnswer(const Outstanding& entry) const
  {
    const CloudSession& session = clouds.at(entry.cloudId).session;
    HttpResponse response;
    response.status = 200;
    response.statusText = "OK";
    response.url = entry.scriptUrl;
    response.headers.push_back({"content-type", "application/json"});
    response.body.assign(session.answer.begin(), session.answer.end());
    return response;
  }

  // Takes a request that waits for the token out of its owner's queue.
  void unqueue(RequestId id, Outstanding& entry)
  {
    if (!entry.queued)
    {
      return;
    }
    entry.queued = false;
    auto owner = owners.find(entry.ownerId);
    if (owner == owners.end())
    {
      return;
    }
    std::deque<RequestId>& waiting = owner->second.waiting;
    for (auto at = waiting.begin(); at != waiting.end(); ++at)
    {
      if (*at == id)
      {
        waiting.erase(at);
        break;
      }
    }
    owner->second.waitingBodyBytes -= entry.request.body.size();
  }

  // The hello came to nothing: every request waiting for its token hears so,
  // as an answer or an error, and the script decides whether to try again.
  // The next rive-cloud: request starts a new hello.
  void failHello(
      CloudEndpointId cloudId,
      const HttpResponse* response,
      const NetError* error)
  {
    CloudState* state = cloudOf(cloudId);
    if (state == nullptr)
    {
      return;
    }
    CloudSession& session = state->session;
    session.helloInFlight = false;
    session.signTicket = 0;
    session.signedAgain = false;
    session.challenge.clear();
    // Only a refusal is a script's to read: any other answer may hold a token.
    NetError unusable;
    if (response != nullptr && response->status < 400)
    {
      unusable.message = "the engine answered the host's hello with a " +
                         std::to_string(response->status) +
                         " the host cannot use";
      response = nullptr;
      error = &unusable;
    }
    for (auto& [id, entry] : outstanding)
    {
      if (!entry.cloud || !entry.queued || entry.settled ||
          entry.cloudId != cloudId)
      {
        continue;
      }
      unqueue(id, entry);
      if (response == nullptr)
      {
        entry.settled = true;
        Outcome outcome;
        outcome.id = id;
        outcome.error = *error;
        queue(std::move(outcome));
        continue;
      }
      HttpResponse answered = *response;
      answered.url = entry.scriptUrl;
      // A hello's 404 or 409 would read as a data request's own answer, so
      // it gets a 401. Too many requests or a failing engine is true of both.
      if (!entry.session && response->status != 429 && response->status < 500)
      {
        answered.status = 401;
        answered.statusText = standardReasonPhrase(401);
      }
      deliverWhole(id, entry, std::move(answered));
    }
  }

  // The engine answered one of the host's hello calls.
  void helloAnswered(
      CloudEndpointId cloudId,
      HelloStep step,
      const HttpResponse& response)
  {
    CloudSession& session = clouds.at(cloudId).session;
    std::vector<std::pair<std::string, std::string>> members;
    bool parsed = jsonMembers(bodyText(response.body), &members);
    std::string challenge = parsed
                                ? plainString(jsonMember(members, "challenge"))
                                : "";
    if (step == HelloStep::Challenge)
    {
      if (response.status != 200 || challenge.empty())
      {
        failHello(cloudId, &response, nullptr);
        return;
      }
      session.challenge = challenge;
      session.helloInFlight = false;
      return;
    }
    if (response.status == 401 && !challenge.empty() && !session.signedAgain)
    {
      // challenge_invalid, with one that is valid.
      session.challenge = challenge;
      session.signedAgain = true;
      session.helloInFlight = false;
      return;
    }
    std::string token = parsed ? plainString(jsonMember(members, "token")) : "";
    if (response.status != 200 || token.empty())
    {
      if (response.status == 400 && session.wantedName == session.helloName)
      {
        // The engine refused the hello as written, which for a hello the
        // host built means the name. Kept, it would fail every later hello,
        // and with them every request.
        session.wantedName.clear();
      }
      failHello(cloudId, &response, nullptr);
      return;
    }
    session.challenge.clear();
    session.signedAgain = false;
    session.helloInFlight = false;
    if (session.wantedName != session.helloName)
    {
      // A script chose a name after this hello left. Its token is dropped
      // and the next hello carries the name, so no request goes out, and no
      // script hears who the player is, under the old one.
      return;
    }
    session.token = std::move(token);
    session.tokenGeneration++;
    session.displayName = jsonMember(members, "display_name");
    session.wantedName.clear();
    session.answer = "{";
    for (const auto& member : members)
    {
      if (member.first == "\"token\"")
      {
        continue;
      }
      if (session.answer.size() > 1)
      {
        session.answer += ",";
      }
      session.answer += member.first + ":" + member.second;
    }
    session.answer += "}";
  }

  // A rive-cloud: request came back 401: its token has expired. It goes back
  // to the front of its owner's queue to be sent with the next token, once.
  // False when it cannot, and the 401 is the script's.
  bool retryWithNewToken(
      RequestId id,
      Outstanding& entry,
      const HttpResponse& response)
  {
    auto owner = owners.find(entry.ownerId);
    // A request made for another endpoint keeps that address, which must not
    // see this one's token.
    if (!entry.cloud || entry.retried || entry.cancelled ||
        response.status != 401 || owner == owners.end() || !isCurrent(entry))
    {
      return false;
    }
    CloudSession& session = cloudOf(entry.cloudId)->session;
    std::vector<std::pair<std::string, std::string>> members;
    std::string challenge;
    if (jsonMembers(bodyText(response.body), &members))
    {
      challenge = plainString(jsonMember(members, "challenge"));
    }
    bool current = entry.tokenGeneration == session.tokenGeneration;
    if (challenge.empty() && current && !session.token.empty())
    {
      // Only an expired token's 401 carries a challenge. Without one the route
      // may refuse any token, so a script cannot cost a hello on every call.
      Clock::time_point now = Clock::now();
      if (now < session.nextBlindHello)
      {
        return false;
      }
      session.nextBlindHello = now + kBlindHelloInterval;
    }
    entry.retried = true;
    if (current)
    {
      session.token.clear();
      // Signing the 401's challenge saves the challenge call.
      if (!challenge.empty() && !session.helloInFlight)
      {
        session.challenge = std::move(challenge);
      }
    }
    release(entry);
    entry.started = false;
    entry.queued = true;
    owner->second.waiting.push_front(id);
    owner->second.waitingBodyBytes += entry.request.body.size();
    return true;
  }

  void addToken(Outstanding& entry, HttpRequest& request) const
  {
    const CloudSession& session = clouds.at(entry.cloudId).session;
    entry.tokenGeneration = session.tokenGeneration;
    request.headers.push_back({"Authorization", "Bearer " + session.token});
  }

  // Whether `owner` has a slot free and room for more answers than poll()
  // has yet delivered.
  bool canStart(const Owner& owner) const
  {
    // Requests in flight hold a place among the answers, as each will
    // become one.
    return owner.inFlight < limits.maxInFlightPerOwner &&
           owner.undelivered + owner.inFlight < limits.maxUndeliveredPerOwner &&
           owner.undeliveredBytes < limits.maxUndeliveredBytesPerOwner;
  }

  // Takes a slot for the entry about to be handed to the provider.
  void begin(Owner& owner, Outstanding& entry, uint32_t timeoutMs)
  {
    owner.inFlight++;
    entry.counted = true;
    entry.started = true;
    entry.starting = true;
    entry.provider = provider;
    entry.deadline = Clock::now() + std::chrono::milliseconds(timeoutMs) +
                     std::chrono::milliseconds(limits.watchdogGraceMs);
  }

  // Queues the request behind its owner's others; false with `error` set
  // when the queue is full.
  bool enqueue(
      Owner& owner,
      RequestId id,
      Outstanding& entry,
      HttpRequest&& request,
      NetError* error)
  {
    size_t bytes = request.body.size();
    if (owner.waiting.size() >= limits.maxQueuedPerOwner)
    {
      error->code = NetErrorCode::RateLimited;
      error->message = "more than " + std::to_string(limits.maxQueuedPerOwner) +
                       " requests waiting";
      return false;
    }
    if (owner.waitingBodyBytes + bytes > limits.maxQueuedBodyBytesPerOwner)
    {
      error->code = NetErrorCode::RateLimited;
      error->message = "more than " +
                       std::to_string(limits.maxQueuedBodyBytesPerOwner) +
                       " bytes of request bodies waiting";
      return false;
    }
    entry.queued = true;
    entry.request = std::move(request);
    owner.waiting.push_back(id);
    owner.waitingBodyBytes += bytes;
    return true;
  }
};
} // namespace

// Hands `request` to `provider` with the lock released, then aborts it if
// the script cancelled it meanwhile.
static void startRequest(
    Net& net,
    RequestId id,
    const HttpRequest& request,
    rcp<Provider> provider)
{
  {
    std::lock_guard<std::mutex> lock(net.mutex);
    auto it = net.outstanding.find(id);
    if (it == net.outstanding.end())
    {
      return;
    }
    // Cancelled while an earlier request in its batch was starting.
    if (it->second.cancelled)
    {
      net.erase(it);
      return;
    }
    // The host named another endpoint after this one was readied for the last.
    if (it->second.cloud && !net.isCurrent(it->second))
    {
      it->second.starting = false;
      net.settle(id, it->second, endpointReplaced());
      return;
    }
  }
  provider->start(id, request);
  bool abortNow = false;
  {
    std::lock_guard<std::mutex> lock(net.mutex);
    auto it = net.outstanding.find(id);
    if (it != net.outstanding.end())
    {
      it->second.starting = false;
      if (it->second.cancelled)
      {
        abortNow = Net::withTransport(it->second);
        net.erase(it);
      }
    }
  }
  if (abortNow)
  {
    provider->abort(id);
  }
}

// Sends one of the host's hello calls to `url`, or fails the hello with
// `refusal` when it is not empty. Takes the lock itself.
static void sendHello(
    Net& net,
    CloudEndpointId cloudId,
    uint64_t epoch,
    HelloStep step,
    std::string url,
    std::string body,
    std::string refusalMessage)
{
  HttpRequest request;
  request.method = "POST";
  request.url = std::move(url);
  request.hostChosen = true;
  if (!body.empty())
  {
    request.body.assign(body.begin(), body.end());
    request.headers.push_back({"Content-Type", "application/json"});
  }
  NetError refusal{NetErrorCode::Policy, std::move(refusalMessage)};
  RequestId id;
  rcp<Provider> provider;
  {
    std::lock_guard<std::mutex> lock(net.mutex);
    CloudState* state = net.cloudOf(cloudId);
    if (state == nullptr || state->session.epoch != epoch)
    {
      // The host named another endpoint meanwhile, which reset the session.
      return;
    }
    if (!refusal.message.empty() || net.provider == nullptr)
    {
      if (refusal.message.empty())
      {
        refusal.code = NetErrorCode::Disabled;
        refusal
            .message = "network access is not enabled for scripts in this host";
      }
      net.failHello(cloudId, nullptr, &refusal);
      return;
    }
    request.timeoutMs = net.limits.defaultTimeoutMs;
    id = net.allocateId();
    Outstanding& entry = net.outstanding[id];
    net.outstandingCount.store(
        net.outstanding.size(),
        std::memory_order_relaxed);
    entry.ownerId = kHostOwner;
    entry.thread = std::this_thread::get_id();
    entry.helloStep = step;
    entry.cloudId = cloudId;
    entry.epoch = epoch;
    entry.started = true;
    entry.starting = true;
    entry.provider = net.provider;
    entry.deadline = Clock::now() +
                     std::chrono::milliseconds(request.timeoutMs) +
                     std::chrono::milliseconds(net.limits.watchdogGraceMs);
    provider = net.provider;
  }
  startRequest(net, id, request, provider);
}

// Sends the host's next hello call, if one is due: the challenge, or the
// hello that signs it once the host's key has. Any thread with a request
// waiting for the token calls this, and the answer changes the session from
// whichever thread the transport reports it on. True when it started one.
static bool driveHello(Net& net, CloudEndpointId cloudId)
{
  std::string challenge;
  CloudEndpoint cloud;
  std::string wantedName;
  uint64_t epoch;
  uint64_t ticket = 0;
  {
    std::lock_guard<std::mutex> lock(net.mutex);
    CloudState* state = net.cloudOf(cloudId);
    if (state == nullptr)
    {
      return false;
    }
    CloudSession& session = state->session;
    if (session.helloInFlight || !session.token.empty() ||
        net.provider == nullptr || state->endpoint.baseUrl.empty())
    {
      return false;
    }
    session.helloInFlight = true;
    challenge = session.challenge;
    wantedName = session.wantedName;
    session.helloName = wantedName;
    cloud = state->endpoint;
    epoch = session.epoch;
    if (!challenge.empty() && cloud.signHello)
    {
      ticket = session.signTicket = ++net.lastSignTicket;
      session.signDeadline = Clock::now() + std::chrono::milliseconds(
                                                net.limits.defaultTimeoutMs);
    }
  }
  std::string url = cloud.baseUrl + "/v1/ns/" + cloud.namespaceId + "/hello";
  if (challenge.empty())
  {
    sendHello(
        net,
        cloudId,
        epoch,
        HelloStep::Challenge,
        url + "/challenge",
        "",
        "");
    return true;
  }
  if (!cloud.signHello)
  {
    sendHello(
        net,
        cloudId,
        epoch,
        HelloStep::Hello,
        url,
        "",
        "this host has no device key, so it cannot say hello");
    return true;
  }
  // Signed without the lock: a host may take a while to reach its key.
  cloud.signHello(
      "rive-hello-v1\n" + challenge,
      [&net,
       cloudId,
       epoch,
       ticket,
       url,
       challenge,
       wantedName,
       installId = cloud.installId](
          bool ok,
          std::string jwk,
          std::string signature) {
        {
          std::lock_guard<std::mutex> lock(net.mutex);
          CloudState* state = net.cloudOf(cloudId);
          if (state == nullptr || state->session.signTicket != ticket)
          {
            // The hello failed, timed out or was replaced meanwhile.
            return;
          }
          state->session.signTicket = 0;
        }
        if (!ok)
        {
          sendHello(
              net,
              cloudId,
              epoch,
              HelloStep::Hello,
              url,
              "",
              "the device key could not sign the hello");
          return;
        }
        std::string body = "{\"challenge\":\"" + challenge +
                           "\",\"device\":{\"key\":" + jwk + ",\"sig\":\"" +
                           signature + "\"}";
        if (!plainString("\"" + installId + "\"").empty())
        {
          body += ",\"install_id\":\"" + installId + "\"";
        }
        if (!wantedName.empty())
        {
          body += ",\"display_name\":" + wantedName;
        }
        body += "}";
        sendHello(net, cloudId, epoch, HelloStep::Hello, url, body, "");
      });
  return true;
}

// Starts `thread`'s queued requests while their owners' limits allow, as
// fetch() would have started them, and again after each batch, since a
// transport answering inside start() frees its slot at once.
static void startQueued(Net& net, std::thread::id thread)
{
  for (;;)
  {
    std::vector<std::pair<RequestId, HttpRequest>> ready;
    rcp<Provider> provider;
    // Endpoints with a request waiting for their token.
    std::vector<CloudEndpointId> wantsToken;
    {
      std::lock_guard<std::mutex> lock(net.mutex);
      provider = net.provider;
      for (auto& [ownerId, owner] : net.owners)
      {
        while (!owner.waiting.empty())
        {
          auto it = net.outstanding.find(owner.waiting.front());
          if (it == net.outstanding.end())
          {
            owner.waiting.pop_front();
            continue;
          }
          Outstanding& entry = it->second;
          if (entry.thread != thread)
          {
            break;
          }
          // A rive-cloud: request waits for the host's hello, and holds its
          // owner's later requests behind it, as any queued request does.
          CloudState* state = entry.cloud ? net.cloudOf(entry.cloudId)
                                          : nullptr;
          bool needsToken = state != nullptr && provider != nullptr &&
                            state->session.token.empty();
          if (needsToken)
          {
            // driveHello starts one hello per endpoint however often asked.
            wantsToken.push_back(entry.cloudId);
            break;
          }
          if (!entry.session && provider != nullptr && !net.canStart(owner))
          {
            break;
          }
          owner.waiting.pop_front();
          owner.waitingBodyBytes -= entry.request.body.size();
          entry.queued = false;
          if (provider == nullptr)
          {
            net.settle(
                it->first,
                entry,
                {NetErrorCode::Disabled,
                 "network access is not enabled for scripts in this host"});
            continue;
          }
          if (entry.session)
          {
            net.deliverWhole(it->first, entry, net.sessionAnswer(entry));
            continue;
          }
          // The host may have narrowed what scripts may fetch, such as
          // allowedHosts or general fetch, since this request was queued. The
          // host's own endpoint keeps its rules.
          if (!entry.cloud)
          {
            NetError refusal;
            if (!net.allow(entry.request, entry, &refusal))
            {
              net.settle(it->first, entry, std::move(refusal));
              continue;
            }
          }
          net.begin(owner, entry, entry.request.timeoutMs);
          if (entry.cloud)
          {
            // Kept, to send again should this token have expired.
            ready.push_back({it->first, entry.request});
            net.addToken(entry, ready.back().second);
          }
          else
          {
            ready.push_back({it->first, std::move(entry.request)});
          }
        }
      }
    }
    // A hello call answered inside start() leaves the next step due at once.
    bool saidHello = false;
    for (CloudEndpointId cloudId : wantsToken)
    {
      saidHello = driveHello(net, cloudId) || saidHello;
    }
    if (ready.empty() && !saidHello)
    {
      return;
    }
    for (auto& [id, request] : ready)
    {
      startRequest(net, id, request, provider);
    }
  }
}

// Why no request may go to `endpoint`, or "" when one may. The namespace and
// the address are spliced into every request URL, so neither may change its
// shape; anything subtler is the host's choice, as http: and loopback are.
static std::string endpointProblem(const CloudEndpoint& endpoint)
{
  const std::string& url = endpoint.baseUrl;
  size_t host = url.compare(0, 8, "https://") == 0  ? 8
                : url.compare(0, 7, "http://") == 0 ? 7
                                                    : 0;
  if (host == 0)
  {
    return "the Rive Cloud address must be an http: or https: URL";
  }
  if (url.size() == host || url[host] == '/')
  {
    return "the Rive Cloud address names no host";
  }
  for (size_t i = host; i < url.size(); i++)
  {
    unsigned char c = (unsigned char)url[i];
    if (c <= 0x20 || c >= 0x7f || c == '?' || c == '#' || c == '@' || c == '\\')
    {
      return "the Rive Cloud address may not have a query, a fragment, "
             "credentials or spaces";
    }
  }
  const std::string& id = endpoint.namespaceId;
  // The engine's own limit for a namespace ID.
  bool plain = !id.empty() && id.size() <= 128;
  for (char c : id)
  {
    plain = plain && (std::isalnum((unsigned char)c) || c == '_' || c == '-');
  }
  if (!plain)
  {
    return "a Rive Cloud namespace ID is 1 to 128 letters, digits, '_' or '-'";
  }
  return "";
}

// Checks `endpoint`, turning it off when it is refused.
static bool checkEndpoint(CloudEndpoint& endpoint, std::string* error)
{
  std::string problem = endpoint.baseUrl.empty() ? ""
                                                 : endpointProblem(endpoint);
  if (problem.empty())
  {
    return true;
  }
  if (error != nullptr)
  {
    *error = problem;
  }
  // Off, rather than left on the last endpoint, which the host meant to
  // replace.
  endpoint = CloudEndpoint();
  return false;
}

// Gives `cloudId` this endpoint and a new session. Requests still waiting name
// the last endpoint's address, and must not go there with this one's token.
// Called with the lock held.
static void replaceCloud(
    Net& net,
    CloudEndpointId cloudId,
    CloudEndpoint endpoint)
{
  NetError replaced = endpointReplaced();
  net.failHello(cloudId, nullptr, &replaced);
  while (!endpoint.baseUrl.empty() && endpoint.baseUrl.back() == '/')
  {
    endpoint.baseUrl.pop_back();
  }
  CloudState& state = net.clouds[cloudId];
  state.endpoint = std::move(endpoint);
  // Another endpoint is another player, and its hello starts from nothing.
  // Ids are never reused, so an epoch need only be new for its endpoint.
  uint64_t epoch = state.session.epoch + 1;
  state.session = CloudSession();
  state.session.epoch = epoch;
  std::random_device random;
  static const char kHex[] = "0123456789abcdef";
  for (int i = 0; i < 4; i++)
  {
    uint32_t word = random();
    for (int nibble = 0; nibble < 8; nibble++)
    {
      state.session.keyPrefix += kHex[(word >> (nibble * 4)) & 0xf];
    }
  }
}

bool setCloudEndpoint(CloudEndpoint endpoint, std::string* error)
{
  bool ok = checkEndpoint(endpoint, error);
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  replaceCloud(net, 0, std::move(endpoint));
  return ok;
}

CloudEndpointId addCloudEndpoint(CloudEndpoint endpoint, std::string* error)
{
  checkEndpoint(endpoint, error);
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  CloudEndpointId cloudId = ++net.lastCloudId;
  replaceCloud(net, cloudId, std::move(endpoint));
  return cloudId;
}

void removeCloudEndpoint(CloudEndpointId cloudId)
{
  if (cloudId == 0)
  {
    return;
  }
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  NetError gone{
      NetErrorCode::Aborted,
      "the file that named this Rive Cloud endpoint was released"};
  net.failHello(cloudId, nullptr, &gone);
  // Requests already out settle as they come back: their session is gone,
  // so none is sent again.
  net.clouds.erase(cloudId);
}

void setGeneralFetchEnabled(bool enabled)
{
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  net.generalFetchEnabled = enabled;
}

void setProvider(rcp<Provider> provider)
{
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  net.provider = std::move(provider);
}

bool hasProvider()
{
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  return net.provider != nullptr;
}

void setLimits(const NetLimits& limits)
{
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  net.limits = limits;
}

NetLimits limits()
{
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  return net.limits;
}

RequestId fetch(
    uint64_t ownerId,
    HttpRequest request,
    rcp<FetchListener> listener,
    CloudEndpointId cloudId)
{
  Net& net = Net::instance();
  RequestId id;
  rcp<Provider> provider;
  bool waitsForToken = false;
  bool queued = false;
  bool streams = listener != nullptr && listener->streams();
  {
    std::lock_guard<std::mutex> lock(net.mutex);
    id = net.allocateId();
    Outstanding& entry = net.outstanding[id];
    net.outstandingCount.store(
        net.outstanding.size(),
        std::memory_order_relaxed);
    entry.ownerId = ownerId;
    entry.thread = std::this_thread::get_id();
    entry.listener = std::move(listener);
    entry.cloudId = cloudId;
    NetError error;
    if (!net.allow(request, entry, &error))
    {
      net.settle(id, entry, std::move(error));
      return id;
    }
    entry.listenerStreams = streams;
    entry.streams = streams && !entry.cloud;
    // Past the limits a request waits its turn, as a browser holds those
    // past its connection limit. A rive-cloud: request also waits for the
    // host's token, and a script's hello is answered from the queue.
    Owner& owner = net.owners[ownerId];
    // allow() let a rive-cloud: request through, so its endpoint is here.
    waitsForToken = entry.cloud &&
                    (entry.session ||
                     net.cloudOf(cloudId)->session.token.empty());
    if (waitsForToken || !owner.waiting.empty() || !net.canStart(owner))
    {
      if (!net.enqueue(owner, id, entry, std::move(request), &error))
      {
        net.settle(id, entry, std::move(error));
        return id;
      }
      queued = true;
    }
    else
    {
      net.begin(owner, entry, request.timeoutMs);
      if (entry.cloud)
      {
        // Kept, to send again should this token have expired.
        entry.request = request;
        net.addToken(entry, request);
      }
      provider = net.provider;
    }
  }
  if (queued)
  {
    if (waitsForToken)
    {
      driveHello(net, cloudId);
    }
    return id;
  }
  startRequest(net, id, request, provider);
  return id;
}

bool cancel(RequestId id)
{
  Net& net = Net::instance();
  rcp<FetchListener> listener;
  rcp<Provider> provider;
  {
    std::lock_guard<std::mutex> lock(net.mutex);
    auto it = net.outstanding.find(id);
    if (it == net.outstanding.end() || it->second.cancelled ||
        it->second.helloStep != HelloStep::None)
    {
      return false;
    }
    listener = std::move(it->second.listener);
    if (it->second.starting)
    {
      it->second.cancelled = true;
    }
    else
    {
      if (Net::withTransport(it->second))
      {
        provider = it->second.provider;
      }
      net.erase(it);
    }
  }
  if (provider != nullptr)
  {
    provider->abort(id);
  }
  if (listener != nullptr)
  {
    listener->onCancel();
  }
  return true;
}

void cancelAllForOwner(uint64_t ownerId)
{
  Net& net = Net::instance();
  std::vector<rcp<FetchListener>> listeners;
  std::vector<std::pair<RequestId, rcp<Provider>>> toAbort;
  {
    std::lock_guard<std::mutex> lock(net.mutex);
    for (auto it = net.outstanding.begin(); it != net.outstanding.end();)
    {
      if (it->second.ownerId != ownerId)
      {
        ++it;
        continue;
      }
      listeners.push_back(std::move(it->second.listener));
      if (it->second.starting)
      {
        it->second.cancelled = true;
        ++it;
        continue;
      }
      if (Net::withTransport(it->second))
      {
        toAbort.push_back({it->first, it->second.provider});
      }
      net.erase(it++);
    }
    net.owners.erase(ownerId);
  }
  for (auto& [id, provider] : toAbort)
  {
    if (provider != nullptr)
    {
      provider->abort(id);
    }
  }
  for (auto& listener : listeners)
  {
    if (listener != nullptr)
    {
      listener->onCancel();
    }
  }
}

uint32_t poll(uint32_t maxDeliveries)
{
  Net& net = Net::instance();
  if (net.outstandingCount.load(std::memory_order_relaxed) == 0)
  {
    return 0;
  }
  std::thread::id thread = std::this_thread::get_id();
  std::vector<std::pair<RequestId, rcp<Provider>>> overdue;
  std::vector<RequestId> overdueHellos;
  uint64_t before;
  {
    std::lock_guard<std::mutex> lock(net.mutex);
    Clock::time_point now = Clock::now();
    for (auto& entry : net.outstanding)
    {
      if (entry.second.started && !entry.second.settled &&
          now >= entry.second.deadline)
      {
        overdue.push_back({entry.first, entry.second.provider});
        if (entry.second.helloStep != HelloStep::None)
        {
          // Settled below, where the map can lose it.
          entry.second.settled = true;
          overdueHellos.push_back(entry.first);
          continue;
        }
        net.settle(
            entry.first,
            entry.second,
            {NetErrorCode::Timeout, "no response before the timeout"});
      }
    }
    for (auto& [cloudId, state] : net.clouds)
    {
      if (state.session.signTicket != 0 && now >= state.session.signDeadline)
      {
        NetError error{
            NetErrorCode::Timeout,
            "the device key did not sign the hello in time"};
        net.failHello(cloudId, nullptr, &error);
      }
    }
    for (RequestId id : overdueHellos)
    {
      auto it = net.outstanding.find(id);
      bool current = net.isCurrent(it->second);
      CloudEndpointId cloudId = it->second.cloudId;
      net.erase(it);
      if (current)
      {
        NetError error{NetErrorCode::Timeout, "no response before the timeout"};
        net.failHello(cloudId, nullptr, &error);
      }
    }
    // Outcomes queued while this poll delivers wait for the next one.
    before = net.nextSequence;
  }
  for (auto& [id, provider] : overdue)
  {
    if (provider != nullptr)
    {
      provider->abort(id);
    }
  }
  startQueued(net, thread);
  uint32_t delivered = 0;
  // One outcome per lock, so a listener that cancels a request still due
  // this poll reaches it before it is delivered.
  while (delivered < maxDeliveries)
  {
    rcp<FetchListener> listener;
    Outcome outcome;
    {
      std::lock_guard<std::mutex> lock(net.mutex);
      if (!net.take(thread, before, &listener, &outcome))
      {
        break;
      }
    }
    delivered++;
    if (listener == nullptr)
    {
      continue;
    }
    if (outcome.body)
    {
      listener->onBody();
    }
    else if (outcome.ok)
    {
      listener->onResponse(std::move(outcome.response));
    }
    else
    {
      listener->onError(outcome.error);
    }
  }
  return delivered;
}

bool hasPendingWork()
{
  Net& net = Net::instance();
  if (net.outstandingCount.load(std::memory_order_relaxed) == 0)
  {
    return false;
  }
  std::lock_guard<std::mutex> lock(net.mutex);
  std::thread::id thread = std::this_thread::get_id();
  for (const auto& entry : net.outstanding)
  {
    if (entry.second.thread == thread && Net::owesDelivery(entry.second))
    {
      return true;
    }
  }
  return false;
}

bool hasPendingWorkForOwner(uint64_t ownerId)
{
  Net& net = Net::instance();
  if (net.outstandingCount.load(std::memory_order_relaxed) == 0)
  {
    return false;
  }
  std::lock_guard<std::mutex> lock(net.mutex);
  for (const auto& entry : net.outstanding)
  {
    if (entry.second.ownerId == ownerId && Net::owesDelivery(entry.second))
    {
      return true;
    }
  }
  return false;
}

namespace
{
using Entry = std::unordered_map<RequestId, Outstanding>::iterator;

// A response that does not stream, whole: the host's own answers are acted
// on, and a script's is queued for poll().
void completeWhole(Net& net, Entry it, HttpResponse&& response)
{
  RequestId id = it->first;
  if (it->second.helloStep != HelloStep::None)
  {
    HelloStep step = it->second.helloStep;
    bool current = net.isCurrent(it->second);
    CloudEndpointId cloudId = it->second.cloudId;
    net.erase(it);
    if (current)
    {
      net.helloAnswered(cloudId, step, response);
    }
    return;
  }
  if (net.retryWithNewToken(id, it->second, response))
  {
    return;
  }
  if (it->second.cloud)
  {
    response.url = it->second.scriptUrl;
  }
  if (response.body.size() > net.limits.maxResponseBodyBytes)
  {
    net.settle(id, it->second, net.tooLarge());
    return;
  }
  net.deliverWhole(id, it->second, std::move(response));
}
} // namespace

void forgetRead(RequestId id)
{
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  auto it = net.outstanding.find(id);
  if (it != net.outstanding.end())
  {
    it->second.wantsBody = false;
  }
}

bool waitsOnUnreadBodies(uint64_t ownerId)
{
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  auto owner = net.owners.find(ownerId);
  if (owner == net.owners.end() || owner->second.waiting.empty())
  {
    return false;
  }
  for (const auto& [id, entry] : net.outstanding)
  {
    // One a read waits on is in use, not dropped.
    if (entry.ownerId == ownerId && entry.delivered && !entry.wantsBody &&
        !entry.bodyQueued && (entry.counted || entry.undeliveredBytes != 0))
    {
      return true;
    }
  }
  return false;
}

void complete(RequestId id, HttpResponse&& response)
{
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  auto it = net.outstanding.find(id);
  if (it == net.outstanding.end() || it->second.settled)
  {
    return;
  }
  if (!it->second.streams)
  {
    completeWhole(net, it, std::move(response));
    return;
  }
  std::vector<uint8_t> body = std::move(response.body);
  response.body.clear();
  net.respondStream(id, it->second, std::move(response));
  net.receiveStream(id, it->second, std::move(body));
  net.endStream(id, it->second, nullptr);
}

bool respond(RequestId id, HttpResponse&& head)
{
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  auto it = net.outstanding.find(id);
  if (it == net.outstanding.end())
  {
    return true;
  }
  // Its head is already in, or the request is over: nothing changes.
  if (it->second.settled || it->second.responded)
  {
    return !it->second.streams;
  }
  head.body.clear();
  if (it->second.streams)
  {
    net.respondStream(id, it->second, std::move(head));
    return false;
  }
  it->second.responded = true;
  it->second.response = std::move(head);
  return true;
}

bool receive(RequestId id, const uint8_t* bytes, size_t size)
{
  Net& net = Net::instance();
  rcp<Provider> refused;
  {
    std::lock_guard<std::mutex> lock(net.mutex);
    auto it = net.outstanding.find(id);
    if (it == net.outstanding.end() || !it->second.responded ||
        it->second.ended)
    {
      return false;
    }
    Outstanding& entry = it->second;
    if (entry.streams)
    {
      return net.receiveStream(
          id,
          entry,
          std::vector<uint8_t>(bytes, bytes + size));
    }
    if (entry.settled)
    {
      return false;
    }
    std::vector<uint8_t>& body = entry.response.body;
    if (body.size() + size <= net.limits.maxResponseBodyBytes)
    {
      body.insert(body.end(), bytes, bytes + size);
      return true;
    }
    entry.response = HttpResponse();
    refused = entry.provider;
    net.failWhole(it, net.tooLarge());
  }
  if (refused != nullptr)
  {
    refused->abort(id);
  }
  return false;
}

void finish(RequestId id)
{
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  auto it = net.outstanding.find(id);
  if (it == net.outstanding.end() || !it->second.responded)
  {
    return;
  }
  if (it->second.streams)
  {
    if (!it->second.ended)
    {
      net.endStream(id, it->second, nullptr);
    }
    return;
  }
  if (it->second.settled)
  {
    return;
  }
  // Taken out first: completing can erase the entry, or send its request
  // again for an answer of its own.
  HttpResponse response = std::move(it->second.response);
  it->second.response = HttpResponse();
  it->second.responded = false;
  completeWhole(net, it, std::move(response));
}

void fail(RequestId id, NetError&& error)
{
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  auto it = net.outstanding.find(id);
  if (it == net.outstanding.end())
  {
    return;
  }
  if (it->second.streams && it->second.responded)
  {
    if (!it->second.ended)
    {
      net.endStream(id, it->second, &error);
    }
    return;
  }
  if (it->second.settled)
  {
    return;
  }
  net.failWhole(it, std::move(error));
}

BodyState readBody(RequestId id, std::vector<uint8_t>* bytes, NetError* error)
{
  Net& net = Net::instance();
  rcp<Provider> resumed;
  {
    std::lock_guard<std::mutex> lock(net.mutex);
    auto it = net.outstanding.find(id);
    if (it == net.outstanding.end() || !it->second.delivered)
    {
      *error = {NetErrorCode::Aborted, "the response is no longer being read"};
      return BodyState::Failed;
    }
    Outstanding& entry = it->second;
    if (entry.chunks.empty())
    {
      if (!entry.ended)
      {
        entry.wantsBody = true;
        return BodyState::Pending;
      }
      BodyState state = entry.failed ? BodyState::Failed : BodyState::End;
      if (entry.failed)
      {
        *error = std::move(entry.bodyError);
      }
      net.erase(it);
      return state;
    }
    *bytes = std::move(entry.chunks.front());
    entry.chunks.pop_front();
    for (const std::vector<uint8_t>& chunk : entry.chunks)
    {
      bytes->insert(bytes->end(), chunk.begin(), chunk.end());
    }
    entry.chunks.clear();
    auto owner = net.owners.find(entry.ownerId);
    if (owner != net.owners.end())
    {
      owner->second.undeliveredBytes -= entry.undeliveredBytes;
    }
    entry.undeliveredBytes = 0;
    if (entry.paused && !entry.ended)
    {
      entry.paused = false;
      resumed = entry.provider;
    }
  }
  if (resumed != nullptr)
  {
    resumed->resume(id);
  }
  return BodyState::Data;
}

} // namespace rive::scriptnet

#endif // WITH_RIVE_SCRIPTNET
