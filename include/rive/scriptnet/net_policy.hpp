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
  /// Answers one VM may have that poll() has yet to deliver, counting those
  /// in flight, and their body bytes; while either is reached, its requests
  /// wait to start. A body's size is known only once it arrives, so the
  /// bytes held can exceed the cap by what is in flight, at most
  /// maxInFlightPerOwner times maxResponseBodyBytes.
  uint32_t maxUndeliveredPerOwner = 64;
  uint32_t maxUndeliveredBytesPerOwner = 64 * 1024 * 1024;
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
  /// Hops a transport that follows redirects itself may take for one
  /// request, as a browser allows.
  uint32_t maxRedirects = 20;
  /// Lets a host resolve to a local or private address, which a transport
  /// that checks addresses (Windows) refuses otherwise. For tests against a
  /// server on loopback; off everywhere else.
  bool allowLocalAddresses = false;
};

/// The rules every script request passes before a transport sees it, and
/// again on every redirect a transport follows. The transport parses the
/// URL a second time with its own parser, so this one accepts only a
/// conservative subset: anything it can't vouch for is refused.
///
/// The rules judge the host by its name. What it resolves to is each
/// transport's to check, through checkAddress(), on the address it is about
/// to connect to for every hop: Windows does so from WinHTTP's connecting
/// callback, so a public name whose DNS points at a local or private
/// address (localtest.me, 10.0.0.1.nip.io, an intranet name) is refused
/// before the request is sent, as a browser's private network access check
/// refuses it, and a connection through a proxy is the proxy's to judge.
/// Known gap, to be closed later: the Apple transport does not check yet.
/// There, since only https: is allowed, the server must still hold a valid
/// certificate for that name, which rules out routers and most local
/// devices but not intranet sites with public certificates, and a failed
/// connection still tells a script whether something answered.
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

  /// For a transport that follows redirects itself. Carries `request`, as
  /// last sent, to the hop that a `status` (301, 302, 303, 307 or 308)
  /// response with `location` asks for, the way the Fetch standard does:
  /// the Location, percent-encoded where it is not already ASCII, is
  /// resolved against the request's URL; a 303, or a 301

  /// or 302 answering a POST, continues as a GET without the body and its
  /// Content-* headers; and Authorization is dropped once the hop leaves
  /// the request's origin. The hop then passes check(). Returns false,
  /// leaving `request` as it was, with `error` set when the hop must not
  /// be sent: the `hops` already taken reach limits.maxRedirects, the
  /// Location cannot be resolved, or check() refuses it.
  static bool redirect(
      HttpRequest& request,
      uint16_t status,
      const std::string& location,
      uint32_t hops,
      const NetLimits& limits,
      NetError* error);

  /// The host rules alone, on a lower-cased host without a port. Refuses
  /// IP literals, single-label and local-network names, and every host
  /// Rive operates.
  static bool checkHost(const std::string& host, std::string* reason);

  /// The address rules, for a transport about to connect to `address`, 4
  /// bytes for IPv4 or 16 for IPv6 in network order, for a host the name
  /// rules let through. Refuses loopback, link-local, private (RFC 1918,
  /// the shared CGNAT space, unique local), multicast, unspecified,
  /// documentation and other reserved ranges, and an IPv6 address carrying
  /// an IPv4 one (mapped, NAT64, 6to4) by that address.
  static bool checkAddress(
      const uint8_t* address,
      size_t length,
      std::string* reason);

  /// Names scripts may not set: the Fetch standard's forbidden request
  /// headers plus the method-override family. Authorization is allowed,
  /// since an author's own bearer token is legitimate. Case-insensitive.
  static bool isForbiddenHeader(const std::string& name);
};

} // namespace rive::scriptnet
