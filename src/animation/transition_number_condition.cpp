#include "rive/animation/transition_number_condition.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/transition_condition_op.hpp"

using namespace rive;

bool TransitionNumberCondition::validateInputType(const StateMachineInput* input) const
{
    // A null input is valid as the StateMachine can attempt to limp along if we
    // introduce new input types that old conditions are expected to handle in
    // newer runtimes. The older runtimes will just evaluate them to true.
    return input == nullptr || input->is<StateMachineNumber>();
}

bool TransitionNumberCondition::evaluate(const SMIInput* inputInstance) const
{
    if (inputInstance == nullptr)
    {
        return true;
    }
    auto numberInput = static_cast<const SMINumber*>(inputInstance);

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
    return false;
}
