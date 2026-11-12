#ifndef _RIVE_LISTENER_FLAGS_HPP_
#define _RIVE_LISTENER_FLAGS_HPP_
#include <cstdint>

namespace rive
{
// Bits of StateMachineListener's listenerFlags, described with the property
// in dev/defs/animation/state_machine_listener.json.
enum class ListenerFlags : uint8_t
{
  none = 0,
  // A capture listener with a press type claims the press: normal handling
  // inside its target misses it until the release. Without it, the listener
  // only observes the press. A drag type still takes the gesture once it
  // starts dragging, as any drag in the host does: its drag pass turns off
  // the nested machines under the pointer until the drag ends.
  stopPropagation = 1 << 0,
};
} // namespace rive
#endif
