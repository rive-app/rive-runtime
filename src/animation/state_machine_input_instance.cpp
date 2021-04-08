#include "animation/state_machine_input_instance.hpp"
#include "animation/state_machine_bool.hpp"
#include "animation/state_machine_double.hpp"
#include "animation/state_machine_trigger.hpp"
#include "animation/state_machine_instance.hpp"

using namespace rive;

StateMachineInputInstance::StateMachineInputInstance(
    StateMachineInput* input, StateMachineInstance* machineInstance) :
    m_MachineInstance(machineInstance), m_Input(input)
{
}
void StateMachineInputInstance::valueChanged()
{
	m_MachineInstance->markNeedsAdvance();
}

// bool

StateMachineBoolInstance::StateMachineBoolInstance(
    StateMachineBool* input, StateMachineInstance* machineInstance) :
    StateMachineInputInstance(input, machineInstance), m_Value(input->value())
{
}

void StateMachineBoolInstance::value(bool newValue)
{
	if (m_Value == newValue)
	{
		return;
	}
	m_Value = newValue;
	valueChanged();
}

// number
StateMachineNumberInstance::StateMachineNumberInstance(
    StateMachineDouble* input, StateMachineInstance* machineInstance) :
    StateMachineInputInstance(input, machineInstance), m_Value(input->value())
{
}

void StateMachineNumberInstance::value(float newValue)
{
	if (m_Value == newValue)
	{
		return;
	}
	m_Value = newValue;
	valueChanged();
}

// trigger
StateMachineTriggerInstance::StateMachineTriggerInstance(
    StateMachineTrigger* input, StateMachineInstance* machineInstance) :
    StateMachineInputInstance(input, machineInstance)
{
}

void StateMachineTriggerInstance::fire()
{
	if (m_Triggered)
	{
		return;
	}
	m_Triggered = true;
	valueChanged();
}
