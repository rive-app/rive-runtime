#include "rive/generated/animation/animation_base.hpp"
#include "rive/animation/animation.hpp"

using namespace rive;

Core* AnimationBase::clone() const
{
    auto cloned = new Animation();
    cloned->copy(*this);
    return cloned;
}
