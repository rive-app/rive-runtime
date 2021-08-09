#include "rive/importers/state_machine_importer.hpp"
#include "rive/animation/state_machine.hpp"

using namespace rive;

StateMachineImporter::StateMachineImporter(StateMachine* machine) :
    m_StateMachine(machine)
{
}

void StateMachineImporter::addLayer(StateMachineLayer* layer)
{
	m_StateMachine->addLayer(layer);
}

void StateMachineImporter::addInput(StateMachineInput* input)
{
	m_StateMachine->addInput(input);
}

bool StateMachineImporter::readNullObject()
{
	// Hard assumption that we won't add new layer types...
	m_StateMachine->addInput(nullptr);
	return true;
}

StatusCode StateMachineImporter::resolve() { return StatusCode::Ok; }