#include "rive/animation/transition_value_boolean_comparator.hpp"

using namespace rive;

bool TransitionValueBooleanComparator::compare(TransitionComparator* comparand,
                                               TransitionConditionOp operation,
                                               StateMachineInstance* stateMachineInstance)
{
    if (comparand->is<TransitionValueBooleanComparator>())
    {
        return compareBooleans(value(),
                               comparand->as<TransitionValueBooleanComparator>()->value(),
                               operation);
    }
    return false;
}