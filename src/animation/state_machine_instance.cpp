#include "animation/state_machine_instance.hpp"
#include "animation/state_machine_input.hpp"
#include "animation/state_machine_bool.hpp"
#include "animation/state_machine_double.hpp"
#include "animation/state_machine_trigger.hpp"
#include "animation/state_machine_input_instance.hpp"
#include "animation/state_machine.hpp"

using namespace rive;

StateMachineInstance::StateMachineInstance(StateMachine* machine) :
    m_Machine(machine)
{
	m_InputCount = machine->inputCount();
	m_InputInstances = new StateMachineInputInstance*[m_InputCount];
	for (int i = 0; i < m_InputCount; i++)
	{
		auto input = machine->input(i);
		if (input == nullptr)
		{
			m_InputInstances[i] = nullptr;
			continue;
		}
		switch (input->coreType())
		{
			case StateMachineBool::typeKey:
				m_InputInstances[i] = new StateMachineBoolInstance(
				    input->as<StateMachineBool>(), this);
				break;
			case StateMachineDouble::typeKey:
				m_InputInstances[i] = new StateMachineNumberInstance(
				    input->as<StateMachineDouble>(), this);
				break;
			case StateMachineTrigger::typeKey:
				m_InputInstances[i] = new StateMachineTriggerInstance(
				    input->as<StateMachineTrigger>(), this);
				break;
		}
	}
}

StateMachineInstance::~StateMachineInstance()
{
	for (int i = 0; i < m_InputCount; i++)
	{
		delete m_InputInstances[i];
	}
	delete[] m_InputInstances;
}

bool StateMachineInstance::advance(float seconds) { return true; }

void StateMachineInstance::apply(Artboard* artboard) const {}