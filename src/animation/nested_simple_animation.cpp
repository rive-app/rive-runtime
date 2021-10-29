#include "rive/animation/nested_simple_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"

using namespace rive;

void NestedSimpleAnimation::advance(float elapsedSeconds, Artboard* artboard)
{
	if (m_AnimationInstance != nullptr)
	{
		if (isPlaying())
		{
			m_AnimationInstance->advance(elapsedSeconds * speed());
		}
		m_AnimationInstance->apply(artboard, mix());
	}
}