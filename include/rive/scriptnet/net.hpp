/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/scriptnet/http.hpp"
#include "rive/scriptnet/net_policy.hpp"
#include "rive/refcnt.hpp"

#include <cstdint>
#include <functional>
#include <vector>

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
/// onCancel is called, though a listener that streams() can get onCancel()
/// after its onResponse(), until it has read the body to its end.
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
  /// True to get the response once its headers arrive, without its body,
  /// which readBody() then takes as it arrives. A body read this way is held
  /// to no size limit or timeout here, but for a rive-cloud: answer, which
  /// the host reads whole first; a transport that only reports whole
  /// responses still holds it to its own.
  virtual bool streams() const { return false; }
  /// From poll(), after readBody() answered Pending: more of the body has
  /// arrived, or it has ended or failed.
  virtual void onBody() {}
};

/// A transport, chosen by the host, never by a file. start() runs on the
/// thread that called fetch(); outcomes come back through complete(), or
/// respond(), receive() and finish(), and fail() below, from any thread.
class Provider : public RefCnt<Provider>
{
public:
  virtual ~Provider() = default;
  /// Begin sending `request`, which NetPolicy already checked. The outcome
  /// may be reported from inside this call.
  virtual void start(RequestId id, const HttpRequest& request) = 0;
  /// Best effort; later outcomes for `id` are dropped either way.
  virtual void abort(RequestId id) {}
  /// receive() answered false for `id`, and its body has room again. It can
  /// arrive before that receive() has returned.
  virtual void resume(RequestId id) {}
};

/// This platform's transport, or null where there is none yet. Apple builds
/// get a cookie-less, credential-less NSURLSession; Windows builds the same
/// over WinHTTP, and Linux hosts that link our curl over it. Android builds
/// get the platform's HttpURLConnection once setJavaVM() has named the VM,
/// without cookies or a cache but under the app's other java.net defaults.
/// Nothing is installed by default: a host opts in with
/// setProvider(makePlatformProvider()).
rcp<Provider> makePlatformProvider();

#ifdef __ANDROID__
/// The app's JavaVM (a JavaVM*), which Android's transport sends through.
/// makePlatformProvider() gives null until this names one.
void setJavaVM(void* javaVM);
#endif

/// Replaces the transport. Null (the default) denies every request.
void setProvider(rcp<Provider> provider);

/// Where a script's rive/cloud calls go. The host supplies this at startup and
/// nothing in a file can set it. A script names only a path, as
/// `rive-cloud:/v1/...`: the host fills in the address and the namespace, says
/// hello with the device key, keeps the player token and adds it to each
/// request, saying hello again when the engine answers 401. So a script can
/// neither choose the address nor touch the key or the token. A script's own
/// `POST rive-cloud:/v1/hello`, with an optional `{"display_name": ...}`, is
/// answered by the host with the engine's hello answer less its token. Each
/// `Idempotency-Key` a script sends gains a prefix that differs every run.
struct CloudEndpoint
{
  /// May be http: or a local address, which fetch() refuses: the host chose it.
  std::string baseUrl;
  std::string namespaceId;
  /// A label sent with the hello. The player comes from the device key.
  std::string installId;
  /// What signHello calls once it has signed: the public key as a JWK and the
  /// raw r||s signature in base64url, or ok false. Call it once, from any
  /// thread, inside signHello or later.
  using HelloSigned = std::function<
      void(bool ok, std::string publicJwk, std::string signature)>;
  /// Signs `message` with this device's P-256 key, which never leaves the
  /// host. A web page signs with a Promise, so the answer may come later; a
  /// hello whose signature takes longer than the request timeout fails.
  std::function<void(const std::string& message, HelloSigned done)> signHello;
};
/// Requests made for the last endpoint and not yet sent fail as Aborted.
/// False, with why in `error`, for an endpoint no request may go to: an
/// address that is not a plain http: or https: URL (no query, fragment or
/// credentials), or a namespace ID other than letters, digits, '_' and '-',
/// which every request path carries. A refused endpoint turns rive-cloud: off.
/// An empty baseUrl turns it off too, and is not refused.
bool setCloudEndpoint(CloudEndpoint endpoint, std::string* error = nullptr);

/// Names an endpoint. 0 is the host's, which setCloudEndpoint sets.
using CloudEndpointId = uint64_t;

/// An endpoint of one file's own, so files on one page can reach different
/// namespaces: each endpoint keeps its own session and token, and a request
/// goes to the endpoint fetch() names. Checked as setCloudEndpoint checks; a
/// refused one is still added, turned off, so its file's rive-cloud: requests
/// fail rather than reach another endpoint. Never 0.
CloudEndpointId addCloudEndpoint(
    CloudEndpoint endpoint,
    std::string* error = nullptr);

/// Forgets an endpoint addCloudEndpoint added. Requests for it not yet sent
/// fail as Aborted. Does nothing for 0.
void removeCloudEndpoint(CloudEndpointId id);

/// Whether plain fetch() is allowed. A host can turn on rive-cloud alone, so a
/// file that talks to its own backend gets no general access to the network.
void setGeneralFetchEnabled(bool enabled);
bool hasProvider();

void setLimits(const NetLimits& limits);
NetLimits limits();

/// Runs the policy, then hands the request to the provider, or queues it
/// behind `ownerId`'s others while its limits are reached; poll() starts it
/// once they allow. Always returns a nonzero id: refusals settle through
/// poll() like every other outcome, so a script never sees its promise
/// settle during the fetch() call.
/// A rive-cloud: request goes to `cloud`, the host's endpoint unless its
/// file added its own.
RequestId fetch(
    uint64_t ownerId,
    HttpRequest request,
    rcp<FetchListener> listener,
    CloudEndpointId cloud = 0);

/// Forgets the request and aborts it at the provider. Returns true when the
/// listener got onCancel(), false when it was or is being delivered instead;
/// a response whose body is still being read counts as not delivered.
bool cancel(RequestId id);

/// cancel() for everything `ownerId` started. Call before the owner's Lua
/// state closes.
void cancelAllForOwner(uint64_t ownerId);

/// Times out overdue requests, starts queued ones this thread made as limits
/// allow, then delivers up to `maxDeliveries` outcomes of requests this
/// thread started. Returns how many it delivered.
uint32_t poll(uint32_t maxDeliveries = 16);

/// True while a request this thread started is outstanding or awaiting
/// delivery. A delivered response whose body nothing waits to read is
/// neither.
bool hasPendingWork();

/// hasPendingWork() for the requests `ownerId` started, on any thread.
bool hasPendingWorkForOwner(uint64_t ownerId);

/// True when `ownerId` has a request waiting to start and a response whose
/// body, with no read waiting on it, keeps a slot or bytes of its budget,
/// whether or not that is all that holds the request back. A binding whose
/// responses a collector frees runs it then.
bool waitsOnUnreadBodies(uint64_t ownerId);

/// Any thread. The provider's outcome for `id`; ignored once `id` has been
/// cancelled, timed out or already completed.
void complete(RequestId id, HttpResponse&& response);
void fail(RequestId id, NetError&& error);

/// Any thread. complete() in steps, for a transport that reports a response
/// as it arrives: respond() with the status and headers, receive() for each
/// run of body bytes, then finish(). fail() ends a body part way.
///
/// respond() answers whether the request's timeout still covers the body,
/// which it does unless a listener reads the body as it arrives.
///
/// receive() answers false when the transport is to stop reading: until
/// Provider::resume(), when the body waiting to be read is full, or for good
/// when the request is over, which it may be because receive() aborted it
/// for a body past the size limit.
bool respond(RequestId id, HttpResponse&& head);
bool receive(RequestId id, const uint8_t* bytes, size_t size);
void finish(RequestId id);

enum class BodyState : uint8_t
{
  /// `bytes` holds all that had arrived.
  Data,
  /// Nothing has arrived since the last read; onBody() says when to read.
  Pending,
  /// The whole body has been read.
  End,
  /// The transport failed or the request is gone; `error` says why.
  Failed,
};

/// For a listener that streams(), from its onResponse() on and on that
/// thread: takes what has arrived of `id`'s body. End and Failed forget the
/// request.
BodyState readBody(RequestId id, std::vector<uint8_t>* bytes, NetError* error);

/// Nothing waits any more on the read readBody() answered Pending for, so
/// no onBody() is owed and the request stops counting as pending work.
void forgetRead(RequestId id);

} // namespace rive::scriptnet
