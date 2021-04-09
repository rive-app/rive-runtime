#include "animation/transition_trigger_condition.hpp"
#include "animation/state_machine_input_instance.hpp"
#include "animation/state_machine_trigger.hpp"
#include "animation/transition_condition_op.hpp"

using namespace rive;

bool TransitionTriggerCondition::validateInputType(
    const StateMachineInput* input) const
{
	// A null input is valid as the StateMachine can attempt to limp along if we
	// introduce new input types that old conditions are expected to handle in
	// newer runtimes. The older runtimes will just evaluate them to true.
	return input == nullptr || input->is<StateMachineTrigger>();
}

bool TransitionTriggerCondition::evaluate(
    const StateMachineInputInstance* inputInstance) const
{
	if (inputInstance == nullptr)
	{
		return true;
	}
	auto triggerInput =
	    reinterpret_cast<const StateMachineTriggerInstance*>(inputInstance);

	if (triggerInput->m_Fired)
	{
		return true;
	}
	return false;
}