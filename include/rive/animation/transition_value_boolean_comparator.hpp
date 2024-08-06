#ifndef _RIVE_TRANSITION_VALUE_BOOLEAN_COMPARATOR_HPP_
#define _RIVE_TRANSITION_VALUE_BOOLEAN_COMPARATOR_HPP_
#include "rive/generated/animation/transition_value_boolean_comparator_base.hpp"
#include <stdio.h>
namespace rive
{
class TransitionValueBooleanComparator : public TransitionValueBooleanComparatorBase
{
public:
    bool compare(TransitionComparator* comparand,
                 TransitionConditionOp operation,
                 const StateMachineInstance* stateMachineInstance) override;
};
} // namespace rive

#endif