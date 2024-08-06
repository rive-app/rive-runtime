#include "rive/animation/transition_value_string_comparator.hpp"

using namespace rive;

bool TransitionValueStringComparator::compare(TransitionComparator* comparand,
                                              TransitionConditionOp operation,
                                              const StateMachineInstance* stateMachineInstance)
{
    if (comparand->is<TransitionValueStringComparator>())
    {
        return compareStrings(value(),
                              comparand->as<TransitionValueStringComparator>()->value(),
                              operation);
    }
    return false;
}