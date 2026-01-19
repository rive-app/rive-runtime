#include "rive/generated/animation/scripted_transition_condition_base.hpp"
#include "rive/animation/scripted_transition_condition.hpp"

using namespace rive;

Core* ScriptedTransitionConditionBase::clone() const
{
    auto cloned = new ScriptedTransitionCondition();
    cloned->copy(*this);
    return cloned;
}
