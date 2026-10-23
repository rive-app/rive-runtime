/*
 * Copyright 2026 Rive
 */

#ifdef WITH_RIVE_SCRIPTNET

#include "rive/scriptnet/net.hpp"

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
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

#ifndef __APPLE__
// Only Apple has a transport so far (net_provider_apple.mm). Hosts elsewhere
// keep denying until theirs lands.
rcp<Provider> makePlatformProvider() { return nullptr; }
#endif

namespace
{
using Clock = std::chrono::steady_clock;

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
  // transport answers.
  bool counted = false;
  // An outcome is queued; any later one is dropped.
  bool settled = false;
  // The transport that started it, and so the one to abort it, even after
  // setProvider() swaps transports.
  rcp<Provider> provider;
  // Provider::start() is running without the lock, so the transport may
  // not know the request yet: a cancel meanwhile only marks it, and
  // fetch() aborts once start() has returned.
  bool starting = false;
  bool cancelled = false;
};

struct Owner
{
  // Requests the transport has yet to answer, for the in-flight limit.
  uint32_t inFlight = 0;
  // Start times in the last minute, for the rate limit.
  std::deque<Clock::time_point> recentStarts;
  // Requests waiting for a slot, oldest first, and their bodies' bytes.
  std::deque<RequestId> waiting;
  size_t waitingBodyBytes = 0;
};

struct Outcome
{
  RequestId id = 0;
  uint64_t sequence = 0;
  bool ok = false;
  HttpResponse response;
  NetError error;
};

class Net
{
public:
  static Net& instance()
  {
    static Net net;
    return net;
  }

  // Everything below is guarded by mutex. Listeners and providers are only
  // ever called with it released, since both can re-enter this file.
  std::mutex mutex;
  rcp<Provider> provider;
  NetLimits limits;
  RequestId nextId = 1;
  std::unordered_map<RequestId, Outstanding> outstanding;
  // Outcomes whose request is no longer outstanding, or was cancelled while
  // starting, are dropped when poll() reaches them.
  std::deque<Outcome> outcomes;
  uint64_t nextSequence = 0;
  std::unordered_map<uint64_t, Owner> owners;
  // outstanding.size(), readable without the lock so idle polls stay cheap.
  std::atomic<size_t> outstandingCount{0};

  void erase(std::unordered_map<RequestId, Outstanding>::iterator it)
  {
    release(it->second);
    if (it->second.queued)
    {
      auto owner = owners.find(it->second.ownerId);
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
    release(entry);
    Outcome outcome;
    outcome.id = id;
    outcome.error = std::move(error);
    queue(std::move(outcome));
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
      *listener = std::move(entry->second.listener);
      *outcome = std::move(*it);
      outcomes.erase(it);
      erase(entry);
      return true;
    }
    return false;
  }

  // The host and the policy; fills `error` and returns false to refuse.
  bool allow(HttpRequest& request, NetError* error)
  {
    if (provider == nullptr)
    {
      error->code = NetErrorCode::Disabled;
      error->message = "network access is not enabled for scripts in this host";
      return false;
    }
    return NetPolicy::check(request, limits, error);
  }

  // Whether `owner` has a slot free and, with a rate limit set, room in
  // the last minute.
  bool canStart(Owner& owner, Clock::time_point now)
  {
    if (owner.inFlight >= limits.maxInFlightPerOwner)
    {
      return false;
    }
    if (limits.maxRequestsPerMinutePerOwner == 0)
    {
      return true;
    }
    std::deque<Clock::time_point>& starts = owner.recentStarts;
    while (!starts.empty() && now - starts.front() >= std::chrono::minutes(1))
    {
      starts.pop_front();
    }
    return starts.size() < limits.maxRequestsPerMinutePerOwner;
  }

  // Takes a slot for the entry about to be handed to the provider.
  void begin(Owner& owner, Outstanding& entry, uint32_t timeoutMs)
  {
    Clock::time_point now = Clock::now();
    if (limits.maxRequestsPerMinutePerOwner != 0)
    {
      owner.recentStarts.push_back(now);
    }
    owner.inFlight++;
    entry.counted = true;
    entry.started = true;
    entry.starting = true;
    entry.provider = provider;
    entry.deadline = now + std::chrono::milliseconds(timeoutMs) +
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
        abortNow = !it->second.settled;
        net.erase(it);
      }
    }
  }
  if (abortNow)
  {
    provider->abort(id);
  }
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
    rcp<FetchListener> listener)
{
  Net& net = Net::instance();
  RequestId id;
  rcp<Provider> provider;
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
    NetError error;
    if (!net.allow(request, &error))
    {
      net.settle(id, entry, std::move(error));
      return id;
    }
    // Past the limits a request waits its turn, as a browser holds those
    // past its connection limit.
    Owner& owner = net.owners[ownerId];
    if (!owner.waiting.empty() || !net.canStart(owner, Clock::now()))
    {
      if (!net.enqueue(owner, id, entry, std::move(request), &error))
      {
        net.settle(id, entry, std::move(error));
      }
      return id;
    }
    net.begin(owner, entry, request.timeoutMs);
    provider = net.provider;
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
    if (it == net.outstanding.end() || it->second.cancelled)
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
      if (it->second.started && !it->second.settled)
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
      if (it->second.started && !it->second.settled)
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
  std::vector<std::pair<RequestId, HttpRequest>> ready;
  rcp<Provider> provider;
  uint64_t before;
  {
    std::lock_guard<std::mutex> lock(net.mutex);
    Clock::time_point now = Clock::now();
    // Queued requests start once slots free, on the thread that queued
    // them, as fetch() would have started them.
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
        if (it->second.thread != thread || !net.canStart(owner, now))
        {
          break;
        }
        owner.waiting.pop_front();
        Outstanding& entry = it->second;
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
        net.begin(owner, entry, entry.request.timeoutMs);
        ready.push_back({it->first, std::move(entry.request)});
      }
    }
    for (auto& entry : net.outstanding)
    {
      if (entry.second.started && !entry.second.settled &&
          now >= entry.second.deadline)
      {
        overdue.push_back({entry.first, entry.second.provider});
        net.settle(
            entry.first,
            entry.second,
            {NetErrorCode::Timeout, "no response before the timeout"});
      }
    }
    // Outcomes queued while this poll delivers wait for the next one.
    before = net.nextSequence;
  }
  for (auto& [id, overdueProvider] : overdue)
  {
    if (overdueProvider != nullptr)
    {
      overdueProvider->abort(id);
    }
  }
  for (auto& [id, request] : ready)
  {
    startRequest(net, id, request, provider);
  }
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
    if (outcome.ok)
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
    if (entry.second.thread == thread)
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
    if (entry.second.ownerId == ownerId)
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
  if (response.body.size() > net.limits.maxResponseBodyBytes)
  {
    net.settle(
        id,
        it->second,
        {NetErrorCode::TooLarge,
         "the response body is larger than " +
             std::to_string(net.limits.maxResponseBodyBytes) + " bytes"});
    return;
  }
  it->second.settled = true;
  net.release(it->second);
  Outcome outcome;
  outcome.id = id;
  outcome.ok = true;
  outcome.response = std::move(response);
  net.queue(std::move(outcome));
}

void fail(RequestId id, NetError&& error)
{
  Net& net = Net::instance();
  std::lock_guard<std::mutex> lock(net.mutex);
  auto it = net.outstanding.find(id);
  if (it == net.outstanding.end() || it->second.settled)
  {
    return;
  }
  net.settle(id, it->second, std::move(error));
}

} // namespace rive::scriptnet

#endif // WITH_RIVE_SCRIPTNET
