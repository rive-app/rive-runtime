#include "rive/generated/animation/linear_animation_base.hpp"
#include "rive/animation/linear_animation.hpp"

using namespace rive;

Core* LinearAnimationBase::clone() const
{
    auto cloned = new LinearAnimation();
    cloned->copy(*this);
    return cloned;
}
