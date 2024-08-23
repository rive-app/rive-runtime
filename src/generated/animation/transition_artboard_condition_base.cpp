#include "rive/generated/animation/transition_artboard_condition_base.hpp"
#include "rive/animation/transition_artboard_condition.hpp"

using namespace rive;

Core* TransitionArtboardConditionBase::clone() const
{
    auto cloned = new TransitionArtboardCondition();
    cloned->copy(*this);
    return cloned;
}
