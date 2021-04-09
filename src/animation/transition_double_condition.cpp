#include "animation/transition_double_condition.hpp"
#include "animation/state_machine_input_instance.hpp"
#include "animation/state_machine_double.hpp"
#include "animation/transition_condition_op.hpp"

using namespace rive;

bool TransitionDoubleCondition::validateInputType(
    const StateMachineInput* input) const
{
	// A null input is valid as the StateMachine can attempt to limp along if we
	// introduce new input types that old conditions are expected to handle in
	// newer runtimes. The older runtimes will just evaluate them to true.
	return input == nullptr || input->is<StateMachineDouble>();
}

bool TransitionDoubleCondition::evaluate(
    const StateMachineInputInstance* inputInstance) const
{
	if (inputInstance == nullptr)
	{
		return true;
	}
	auto numberInput =
	    reinterpret_cast<const StateMachineNumberInstance*>(inputInstance);

	switch (op())
	{
		case TransitionConditionOp::equal:
			return numberInput->value() == value();
		case TransitionConditionOp::notEqual:
			return numberInput->value() != value();
		case TransitionConditionOp::lessThanOrEqual:
			return numberInput->value() <= value();
		case TransitionConditionOp::lessThan:
			return numberInput->value() < value();
		case TransitionConditionOp::greaterThanOrEqual:
			return numberInput->value() >= value();
		case TransitionConditionOp::greaterThan:
			return numberInput->value() > value();
	}
}