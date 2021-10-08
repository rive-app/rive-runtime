#include "rive/animation/nested_linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"

using namespace rive;

NestedLinearAnimation::~NestedLinearAnimation() { delete m_AnimationInstance; }

void NestedLinearAnimation::initializeAnimation(Artboard* artboard)
{
	assert(m_AnimationInstance == nullptr);
	m_AnimationInstance =
	    new LinearAnimationInstance(artboard->animation(animationId()));
}