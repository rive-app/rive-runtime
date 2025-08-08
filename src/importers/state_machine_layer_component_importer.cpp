#include "rive/importers/state_machine_layer_component_importer.hpp"
#include "rive/animation/state_machine_layer_component.hpp"
#include "rive/animation/state_machine_fire_action.hpp"

using namespace rive;

StateMachineLayerComponent::~StateMachineLayerComponent()
{
    for (auto event : m_events)
    {
        delete event;
    }
}

StateMachineLayerComponentImporter::StateMachineLayerComponentImporter(
    StateMachineLayerComponent* component) :
    m_stateMachineLayerComponent(component)
{}

void StateMachineLayerComponentImporter::addFireEvent(
    StateMachineFireAction* fireEvent)
{
    m_stateMachineLayerComponent->m_events.push_back(fireEvent);
}