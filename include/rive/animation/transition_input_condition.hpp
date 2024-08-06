#ifndef _RIVE_TRANSITION_INPUT_CONDITION_HPP_
#define _RIVE_TRANSITION_INPUT_CONDITION_HPP_
#include "rive/generated/animation/transition_input_condition_base.hpp"
#include <stdio.h>
namespace rive
{
class TransitionInputCondition : public TransitionInputConditionBase
{
public:
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif