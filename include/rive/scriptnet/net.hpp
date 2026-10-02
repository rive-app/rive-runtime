/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/scriptnet/http.hpp"
#include "rive/scriptnet/net_policy.hpp"
#include "rive/refcnt.hpp"

#include <cstdint>

// Script networking. A script's fetch() becomes a request here; NetPolicy and
// the per-VM limits run first, then the host's Provider sends it, and the
// outcome is delivered by poll(), which rive_pollAsyncWork() drains each
// frame. Promises therefore settle inside the advance window, never from a
// transport callback.
//
// Outcomes are delivered only on the thread that started the request, so a
// VM living on a worker thread is never touched by another thread's poll.
//
// Not rive::net: that namespace belongs to packages/rive_net (the backend
// client), which hosts like rive-cli link alongside the runtime.
namespace rive::scriptnet
{

using RequestId = uint32_t;

/// Receives the outcome of one request. Exactly one of onResponse, onError or
/// onCancel is called.
class FetchListener : public RefCnt<FetchListener>
{
public:
    virtual ~FetchListener() = default;
    /// From poll(), on the thread that started the request.
    virtual void onResponse(HttpResponse&& response) = 0;
    /// From poll(), on the thread that started the request.
    virtual void onError(const NetError& error) = 0;
    /// From cancel() or cancelAllForOwner(), possibly while the owner's VM is
    /// closing, so it must not call into Lua.
    virtual void onCancel() {}
};

/// A transport, chosen by the host, never by a file. start() runs on the
/// thread that called fetch(); outcomes come back through complete() and
/// fail() below, from any thread.
class Provider : public RefCnt<Provider>
{
public:
    virtual ~Provider() = default;
    /// Begin sending `request`, which NetPolicy already checked. The outcome
    /// may be reported from inside this call.
    virtual void start(RequestId id, const HttpRequest& request) = 0;
    /// Best effort; later outcomes for `id` are dropped either way.
    virtual void abort(RequestId id) {}
};

/// This platform's transport, or null where there is none yet. Apple builds
/// get a cookie-less, credential-less NSURLSession. Nothing is installed by
/// default: a host opts in with setProvider(makePlatformProvider()).
rcp<Provider> makePlatformProvider();

/// Replaces the transport. Null (the default) denies every request.
void setProvider(rcp<Provider> provider);
bool hasProvider();

void setLimits(const NetLimits& limits);
NetLimits limits();

/// Runs the policy and `ownerId`'s limits, then hands the request to the
/// provider. Always returns a nonzero id: refusals settle through poll() like
/// every other outcome, so a script never sees its promise settle during the
/// fetch() call.
RequestId fetch(uint64_t ownerId,
                HttpRequest request,
                rcp<FetchListener> listener);

/// Forgets the request and aborts it at the provider. Returns true when the
/// listener got onCancel(), false when it was or is being delivered instead.
bool cancel(RequestId id);

/// cancel() for everything `ownerId` started. Call before the owner's Lua
/// state closes.
void cancelAllForOwner(uint64_t ownerId);

/// Times out overdue requests, then delivers up to `maxDeliveries` outcomes of
/// requests this thread started. Returns how many it delivered.
uint32_t poll(uint32_t maxDeliveries = 16);

/// True while a request this thread started is outstanding or awaiting
/// delivery.
bool hasPendingWork();

/// Any thread. The provider's outcome for `id`; ignored once `id` has been
/// cancelled, timed out or already completed.
void complete(RequestId id, HttpResponse&& response);
void fail(RequestId id, NetError&& error);

} // namespace rive::scriptnet
