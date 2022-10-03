#include "rive/generated/animation/animation_state_base.hpp"
#include "rive/animation/animation_state.hpp"

using namespace rive;

Core* AnimationStateBase::clone() const
{
    auto cloned = new AnimationState();
    cloned->copy(*this);
    return cloned;
}
