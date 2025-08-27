#include "rive/generated/animation/transition_viewmodel_condition_base.hpp"
#include "rive/animation/transition_viewmodel_condition.hpp"

using namespace rive;

Core* TransitionViewModelConditionBase::clone() const
{
    auto cloned = new TransitionViewModelCondition();
    cloned->copy(*this);
    return cloned;
}
