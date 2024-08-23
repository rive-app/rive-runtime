#include "rive/generated/animation/blend_animation_direct_base.hpp"
#include "rive/animation/blend_animation_direct.hpp"

using namespace rive;

Core* BlendAnimationDirectBase::clone() const
{
    auto cloned = new BlendAnimationDirect();
    cloned->copy(*this);
    return cloned;
}
