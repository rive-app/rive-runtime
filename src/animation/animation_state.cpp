#include "rive/animation/animation_state.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/animation_state_instance.hpp"
#include "rive/animation/system_state_instance.hpp"
#include "rive/core_context.hpp"
#include "rive/artboard.hpp"

using namespace rive;

StateInstance* AnimationState::makeInstance() const
{
	if (animation() == nullptr)
	{
		// Failed to load at runtime/some new type we don't understand.
		return new SystemStateInstance(this);
	}
	return new AnimationStateInstance(this);
}