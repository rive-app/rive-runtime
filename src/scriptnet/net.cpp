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
    for (NetErrorCode code : {NetErrorCode::Disabled,
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
    // Handed to the provider, as opposed to refused up front.
    bool started = false;
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
    // Started requests still in outstanding, for the in-flight limit.
    uint32_t inFlight = 0;
    // Start times in the last minute, for the rate limit.
    std::deque<Clock::time_point> recentStarts;
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
        if (it->second.started)
        {
            auto owner = owners.find(it->second.ownerId);
            if (owner != owners.end() && owner->second.inFlight > 0)
            {
                owner->second.inFlight--;
            }
        }
        outstanding.erase(it);
        outstandingCount.store(outstanding.size(), std::memory_order_relaxed);
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
        Outcome outcome;
        outcome.id = id;
        outcome.error = std::move(error);
        queue(std::move(outcome));
    }

    // Detaches the first deliverable outcome for `thread` queued before
    // `before`.
    bool take(std::thread::id thread,
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

    // Policy and limits; fills `error` and returns false to refuse.
    bool admit(uint64_t ownerId, HttpRequest& request, NetError* error)
    {
        if (provider == nullptr)
        {
            error->code = NetErrorCode::Disabled;
            error->message =
                "network access is not enabled for scripts in this host";
            return false;
        }
        if (!NetPolicy::check(request, limits, error))
        {
            return false;
        }
        Owner& owner = owners[ownerId];
        if (owner.inFlight >= limits.maxInFlightPerOwner)
        {
            error->code = NetErrorCode::RateLimited;
            error->message = "more than " +
                             std::to_string(limits.maxInFlightPerOwner) +
                             " requests in flight";
            return false;
        }
        Clock::time_point now = Clock::now();
        std::deque<Clock::time_point>& starts = owner.recentStarts;
        while (!starts.empty() &&
               now - starts.front() >= std::chrono::minutes(1))
        {
            starts.pop_front();
        }
        if (starts.size() >= limits.maxRequestsPerMinutePerOwner)
        {
            error->code = NetErrorCode::RateLimited;
            error->message =
                "more than " +
                std::to_string(limits.maxRequestsPerMinutePerOwner) +
                " requests in the last minute";
            return false;
        }
        starts.push_back(now);
        owner.inFlight++;
        return true;
    }
};
} // namespace

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

RequestId fetch(uint64_t ownerId,
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
        net.outstandingCount.store(net.outstanding.size(),
                                   std::memory_order_relaxed);
        entry.ownerId = ownerId;
        entry.thread = std::this_thread::get_id();
        entry.listener = std::move(listener);
        NetError error;
        if (!net.admit(ownerId, request, &error))
        {
            net.settle(id, entry, std::move(error));
            return id;
        }
        entry.started = true;
        entry.starting = true;
        entry.provider = net.provider;
        entry.deadline = Clock::now() +
                         std::chrono::milliseconds(request.timeoutMs) +
                         std::chrono::milliseconds(net.limits.watchdogGraceMs);
        provider = net.provider;
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
                abortNow = !it->second.settled;
                net.erase(it);
            }
        }
    }
    if (abortNow)
    {
        provider->abort(id);
    }
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
    std::vector<std::pair<RequestId, rcp<Provider>>> overdue;
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
                net.settle(
                    entry.first,
                    entry.second,
                    {NetErrorCode::Timeout, "no response before the timeout"});
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
    std::thread::id thread = std::this_thread::get_id();
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
        net.settle(id,
                   it->second,
                   {NetErrorCode::TooLarge,
                    "the response body is larger than " +
                        std::to_string(net.limits.maxResponseBodyBytes) +
                        " bytes"});
        return;
    }
    it->second.settled = true;
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
