#include "rive/animation/nested_linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"

using namespace rive;

NestedLinearAnimation::NestedLinearAnimation() {}
NestedLinearAnimation::~NestedLinearAnimation() {}

void NestedLinearAnimation::initializeAnimation(ArtboardInstance* artboard)
{
    m_AnimationInstance =
        rivestd::make_unique<LinearAnimationInstance>(artboard->animation(animationId()), artboard);
}