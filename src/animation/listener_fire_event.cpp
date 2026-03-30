#include "rive/animation/listener_fire_event.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/event.hpp"

using namespace rive;

void ListenerFireEvent::perform(StateMachineInstance* stateMachineInstance,
                                const ListenerInvocation& invocation) const
{
    (void)invocation;
    auto coreEvent = stateMachineInstance->artboard()->resolve(eventId());
    if (coreEvent == nullptr || !coreEvent->is<Event>())
    {
        return;
    }
    stateMachineInstance->reportEvent(coreEvent->as<Event>());
}