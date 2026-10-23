#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/wasm_signal_wait.hpp"
#include "rive/async/timeouts.hpp"
#include "rive/wasm/wasm_lane_layout_gen.hpp"
#include "rive/wasm/wasm_shared.hpp"

using namespace rive;

namespace
{
// Casting a double outside the range to an integer is undefined.
uint64_t sequenceAfter(double after)
{
  if (!(after > 0))
  {
    return 0;
  }
  return after >= 18446744073709551616.0 ? UINT64_MAX : (uint64_t)after;
}
} // namespace

SignalWait::SignalWait(
    uint32_t id,
    uint64_t owner,
    uint32_t signal,
    const SharedSignal* found,
    double after,
    double timeoutSeconds)
    : id(id)
    , owner(owner)
    , signal(signal)
    , after(sequenceAfter(after))
    , deadline(
          unboundedSeconds(timeoutSeconds) ? Clock::time_point::max()
                                           : deadlineAfter(timeoutSeconds))
    , gone(signal != 0 && found == nullptr)
{}

rcp<SharedSignal> SignalWait::find(uint32_t signal, uint64_t owner)
{
  return signal != 0 ? SharedRegistry::find<SharedSignal>(signal, owner)
                     : nullptr;
}

bool SignalWait::fired(Clock::time_point now, double& value) const
{
  if (signal != 0)
  {
    rcp<SharedSignal> found = find(signal, owner);
    // Gone mid wait settles only an unbounded wait, so a timed one times
    // out however soon its handle is collected.
    if (found == nullptr && (gone || deadline == Clock::time_point::max()))
    {
      value = lane_layout::kWaitGone;
      return true;
    }
    if (found != nullptr && found->seq() > after)
    {
      value = (double)found->seq();
      return true;
    }
  }
  if (now >= deadline)
  {
    value = lane_layout::kWaitTimedOut;
    return true;
  }
  return false;
}

double SignalWait::block() const
{
  rcp<SharedSignal> watched = find(signal, owner);
  if (watched == nullptr)
  {
    return lane_layout::kWaitGone;
  }
  double value = lane_layout::kWaitTimedOut;
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  fired(deadline, value);
#else
  thread_local WakeWord word;
  watched->addWatcher(&word);
  // The word is read first, so a notify after the check still wakes it.
  for (uint32_t seen = word.value(); !fired(Clock::now(), value);
       seen = word.value())
  {
    word.wait(seen, deadline);
  }
  watched->removeWatcher(&word);
#endif
  return value;
}

#endif
