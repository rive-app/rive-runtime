#include "rive/generated/animation/transition_trigger_condition_base.hpp"
#include "rive/animation/transition_trigger_condition.hpp"

using namespace rive;

Core* TransitionTriggerConditionBase::clone() const
{
    auto cloned = new TransitionTriggerCondition();
    cloned->copy(*this);
    return cloned;
}
