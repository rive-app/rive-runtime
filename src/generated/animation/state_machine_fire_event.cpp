#include "rive/generated/animation/state_machine_fire_event_base.hpp"
#include "rive/animation/state_machine_fire_event.hpp"
#include "rive/animation/state_machine_layer_component.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/event.hpp"
#include "rive/importers/state_machine_layer_component_importer.hpp"

using namespace rive;

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