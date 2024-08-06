#include "rive/animation/transition_value_color_comparator.hpp"

using namespace rive;

bool TransitionValueColorComparator::compare(TransitionComparator* comparand,
                                             TransitionConditionOp operation,
                                             const StateMachineInstance* stateMachineInstance)
{
    if (comparand->is<TransitionValueColorComparator>())
    {
        return compareColors(value(),
                             comparand->as<TransitionValueColorComparator>()->value(),
                             operation);
    }
    return false;
}