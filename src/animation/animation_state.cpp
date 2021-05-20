#include "animation/animation_state.hpp"
#include "animation/linear_animation.hpp"
#include "animation/animation_state_instance.hpp"
#include "animation/system_state_instance.hpp"
#include "core_context.hpp"
#include "artboard.hpp"

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