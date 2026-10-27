#ifndef _RIVE_LISTENER_PHASE_HPP_
#define _RIVE_LISTENER_PHASE_HPP_
#include <cstdint>

namespace rive
{
// Mirrored as phaseValue's enumValues in
// dev/defs/animation/state_machine_listener.json.
enum class ListenerPhase : uint32_t
{
  // Runs with the target's own handling: keys bubble from the focused node
  // outward, pointers walk the scene topmost-first.
  normal = 0,
  // Runs before every normal listener, outermost artboard and ancestor
  // first. A capture listener that asks for an exact key, or for a press,
  // claims it: normal handling inside its target never sees it.
  capture = 1,
};
} // namespace rive
#endif
