#include "rive/generated/animation/state_machine_fire_action_base.hpp"
#include "rive/animation/state_machine_fire_event.hpp"
#include "rive/animation/state_machine_layer_component.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/event.hpp"
#include "rive/importers/state_machine_layer_component_importer.hpp"

using namespace rive;

StatusCode StateMachineFireAction::import(ImportStack& importStack)
{
    auto stateImporter = importStack.latest<StateMachineLayerComponentImporter>(
        StateMachineLayerComponent::typeKey);
    if (stateImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    stateImporter->addFireEvent(this);
    return Super::import(importStack);
}