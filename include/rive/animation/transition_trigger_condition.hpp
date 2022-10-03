#ifndef _RIVE_TRANSITION_TRIGGER_CONDITION_HPP_
#define _RIVE_TRANSITION_TRIGGER_CONDITION_HPP_
#include "rive/generated/animation/transition_trigger_condition_base.hpp"
#include <stdio.h>
namespace rive
{
class TransitionTriggerCondition : public TransitionTriggerConditionBase
{
public:
    bool evaluate(const SMIInput* inputInstance) const override;

protected:
    bool validateInputType(const StateMachineInput* input) const override;
};
} // namespace rive

#endif