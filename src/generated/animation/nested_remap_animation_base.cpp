#include "rive/generated/animation/nested_remap_animation_base.hpp"
#include "rive/animation/nested_remap_animation.hpp"

using namespace rive;

Core* NestedRemapAnimationBase::clone() const
{
    auto cloned = new NestedRemapAnimation();
    cloned->copy(*this);
    return cloned;
}
