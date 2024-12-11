#include "rive/animation/transition_value_number_comparator.hpp"

using namespace rive;

bool TransitionValueNumberComparator::compare(
    TransitionComparator* comparand,
    TransitionConditionOp operation,
    const StateMachineInstance* stateMachineInstance,
    bool ignoreTriggers)
{
    if (comparand->is<TransitionValueNumberComparator>())
    {
        return compareNumbers(
            value(),
            comparand->as<TransitionValueNumberComparator>()->value(),
            operation);
    }
    return false;
}