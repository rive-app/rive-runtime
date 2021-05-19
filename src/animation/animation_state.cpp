#include "animation/animation_state.hpp"
#include "animation/linear_animation.hpp"
#include "animation/animation_state_instance.hpp"
#include "core_context.hpp"
#include "artboard.hpp"

using namespace rive;

StateInstance* AnimationState::makeInstance() const
{
	return new AnimationStateInstance(this);
}