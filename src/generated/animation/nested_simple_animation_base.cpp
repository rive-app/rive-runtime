#include "rive/generated/animation/nested_simple_animation_base.hpp"
#include "rive/animation/nested_simple_animation.hpp"

using namespace rive;

Core* NestedSimpleAnimationBase::clone() const
{
    auto cloned = new NestedSimpleAnimation();
    cloned->copy(*this);
    return cloned;
}
