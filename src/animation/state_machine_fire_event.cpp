#include "rive/animation/state_machine_fire_event.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/event.hpp"

using namespace rive;

// Anchors StateMachineFireEvent's vtable (its first non-inline virtual).
// Previously this definition lived in an untracked file under
// src/generated/, which `generate_core.sh` wipes — leaving the runtime
// (LTO) link with an undefined vtable for StateMachineFireEvent. Kept
// here in src/animation/ alongside its siblings (fire_action,
// fire_trigger) so regeneration can't remove it again.
void StateMachineFireEvent::perform(
    StateMachineInstance* stateMachineInstance) const
{
    auto coreEvent = stateMachineInstance->artboard()->resolve(eventId());
    if (coreEvent == nullptr || !coreEvent->is<Event>())
    {
        return;
    }
    stateMachineInstance->reportEvent(coreEvent->as<Event>());
}
