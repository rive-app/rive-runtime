#ifndef _RIVE_TRANSITION_BOOL_CONDITION_HPP_
#define _RIVE_TRANSITION_BOOL_CONDITION_HPP_
#include "rive/generated/animation/transition_bool_condition_base.hpp"
#include <stdio.h>
namespace rive
{
class TransitionBoolCondition : public TransitionBoolConditionBase
{
public:
    bool evaluate(const StateMachineInstance* stateMachineInstance) const override;

protected:
    bool validateInputType(const StateMachineInput* input) const override;
};
} // namespace rive

#endif