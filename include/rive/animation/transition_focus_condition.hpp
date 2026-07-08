#ifndef _RIVE_TRANSITION_FOCUS_CONDITION_HPP_
#define _RIVE_TRANSITION_FOCUS_CONDITION_HPP_
#include "rive/generated/animation/transition_focus_condition_base.hpp"
#include <stdio.h>
namespace rive
{
// Condition that evaluates whether a specific focusable component currently
// holds focus. The target component is stored on the right comparator (a
// TransitionPropertyComponentComparator pointing at the focusable node); the
// left side is the implicit "current focus". Only ==/!= are meaningful.
class TransitionFocusCondition : public TransitionFocusConditionBase
{
public:
    bool evaluate(const StateMachineInstance* stateMachineInstance,
                  StateMachineLayerInstance* layerInstance) const override;
};
} // namespace rive

#endif
