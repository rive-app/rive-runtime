/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/scriptnet/http.hpp"

#include <string>

namespace rive::scriptnet
{

/// Host-configurable ceilings. Requests past the in-flight or rate limit wait
/// in their VM's queue and start in order, as a browser holds requests past
/// its connection limit; only a full queue, the policy or the size limits
/// reject one.
struct NetLimits
{
  /// Requests one VM may have with the transport at once.
  uint32_t maxInFlightPerOwner = 8;
  /// Requests one VM may start in any 60 second window, 0 for no limit.
  uint32_t maxRequestsPerMinutePerOwner = 0;
  /// Requests one VM may have waiting, and the request body bytes they may
  /// hold together; past either, a request is rejected.
  uint32_t maxQueuedPerOwner = 1024;
  uint32_t maxQueuedBodyBytesPerOwner = 32 * 1024 * 1024;
  uint32_t maxRequestBodyBytes = 8 * 1024 * 1024;
  uint32_t maxResponseBodyBytes = 16 * 1024 * 1024;
  uint32_t defaultTimeoutMs = 30 * 1000;
  uint32_t maxTimeoutMs = 60 * 1000;
  /// How long past its own timeout a request may go unanswered before
  /// poll() stops waiting on the provider. Providers enforce the timeout
  /// themselves; this only catches one that never answers.
  uint32_t watchdogGraceMs = 5 * 1000;
  uint32_t maxUrlLength = 8 * 1024;
  uint32_t maxHeaderCount = 64;
  uint32_t maxHeaderBytes = 16 * 1024;
};

/// The rules every script request passes before a transport sees it, and
/// again on every redirect a transport follows. The transport parses the
/// URL a second time with its own parser, so this one accepts only a
/// conservative subset: anything it can't vouch for is refused.
///
/// Known gap, to be closed later: the rules judge the host by its name, not
/// by the address it resolves to. A public name whose DNS points at a local
/// or private address (localtest.me, 10.0.0.1.nip.io, an intranet name)
/// passes. Since only https: is allowed, the server must still hold a valid
/// certificate for that name, which rules out routers and most local
/// devices but not intranet sites with public certificates, and a failed
/// connection still tells a script whether something answered. The plan:
/// each transport resolves the host before connecting and before following
/// a redirect, and refuses local and private addresses through a check
/// here that the transports share.
class NetPolicy
{
public:
  /// Normalizes `request` in place (method upper-cased, host lower-cased,
  /// fragment dropped, timeout defaulted and clamped). Returns false with
  /// `error` set when the request must not be sent.
  static bool check(
      HttpRequest& request,
      const NetLimits& limits,
      NetError* error);

  /// The host rules alone, on a lower-cased host without a port. Refuses
  /// IP literals, single-label and local-network names, and every host
  /// Rive operates.
  static bool checkHost(const std::string& host, std::string* reason);

  /// Names scripts may not set: the Fetch standard's forbidden request
  /// headers plus the method-override family. Authorization is allowed,
  /// since an author's own bearer token is legitimate. Case-insensitive.
  static bool isForbiddenHeader(const std::string& name);
};

} // namespace rive::scriptnet
