#ifndef _RIVE_TRANSITION_NUMBER_CONDITION_HPP_
#define _RIVE_TRANSITION_NUMBER_CONDITION_HPP_
#include "rive/generated/animation/transition_number_condition_base.hpp"
#include <stdio.h>
namespace rive
{
class TransitionNumberCondition : public TransitionNumberConditionBase
{
protected:
    bool validateInputType(const StateMachineInput* input) const override;

public:
    bool evaluate(const StateMachineInstance* stateMachineInstance) const override;
};
} // namespace rive

#endif