#include "rive/importers/state_machine_importer.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/animation/state_machine_input.hpp"
#include "rive/animation/state_machine_layer.hpp"

using namespace rive;

StateMachineImporter::StateMachineImporter(StateMachine* machine) : m_StateMachine(machine) {}

void StateMachineImporter::addLayer(std::unique_ptr<StateMachineLayer> layer)
{
    m_StateMachine->addLayer(std::move(layer));
}

void StateMachineImporter::addInput(std::unique_ptr<StateMachineInput> input)
{
    m_StateMachine->addInput(std::move(input));
}

void StateMachineImporter::addListener(std::unique_ptr<StateMachineListener> listener)
{
    m_StateMachine->addListener(std::move(listener));
}

bool StateMachineImporter::readNullObject()
{
    // Hard assumption that we won't add new layer types...
    m_StateMachine->addInput(nullptr);
    return true;
}

StatusCode StateMachineImporter::resolve() { return StatusCode::Ok; }