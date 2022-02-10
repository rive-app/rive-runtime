#include "rive/generated/animation/transition_bool_condition_base.hpp"
#include "rive/animation/transition_bool_condition.hpp"

using namespace rive;

Core* TransitionBoolConditionBase::clone() const
{
    auto cloned = new TransitionBoolCondition();
    cloned->copy(*this);
    return cloned;
}
