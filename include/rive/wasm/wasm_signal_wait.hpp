#ifndef _RIVE_WASM_SIGNAL_WAIT_HPP_
#define _RIVE_WASM_SIGNAL_WAIT_HPP_

#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/refcnt.hpp"

#include <chrono>
#include <cstdint>

namespace rive
{
class SharedSignal;

/// An await on a shared signal, on the main lane or in a worker. It names the
/// signal by id, so it sees the signal go mid wait; signal 0 is a sleep.
struct SignalWait
{
  using Clock = std::chrono::steady_clock;

  /// Only a token, for an await on something other than a signal.
  explicit SignalWait(uint32_t id) : id(id) {}
  /// A wait on signal, which found is when it was there as the wait began.
  SignalWait(
      uint32_t id,
      uint64_t owner,
      uint32_t signal,
      const SharedSignal* found,
      double after,
      double timeoutSeconds);

  /// The signal owner's VM made under id, null for 0 or once it is gone.
  static rcp<SharedSignal> find(uint32_t signal, uint64_t owner);
  /// Whether it settles at now, with the wake value it settles with.
  bool fired(Clock::time_point now, double& value) const;
  /// Blocks this thread until it settles: the value it settles with.
  /// Without threads nothing could notify meanwhile, so it never waits.
  double block() const;

  uint32_t id;
  uint64_t owner = 0;
  uint32_t signal = 0;
  uint64_t after = 0;
  /// The time point max when unbounded.
  Clock::time_point deadline = Clock::time_point::max();
  Clock::time_point since = Clock::now();
  /// Its signal was gone when it began.
  bool gone = false;
};
} // namespace rive

#endif
#endif
