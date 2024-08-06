#ifndef _RIVE_TRANSITION_VALUE_COLOR_COMPARATOR_HPP_
#define _RIVE_TRANSITION_VALUE_COLOR_COMPARATOR_HPP_
#include "rive/generated/animation/transition_value_color_comparator_base.hpp"
#include <stdio.h>
namespace rive
{
class TransitionValueColorComparator : public TransitionValueColorComparatorBase
{
public:
    bool compare(TransitionComparator* comparand,
                 TransitionConditionOp operation,
                 const StateMachineInstance* stateMachineInstance) override;
};
} // namespace rive

#endif