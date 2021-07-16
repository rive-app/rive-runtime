#include "generated/animation/blend_state_transition_base.hpp"
#include "animation/blend_state_transition.hpp"

using namespace rive;

Core* BlendStateTransitionBase::clone() const
{
	auto cloned = new BlendStateTransition();
	cloned->copy(*this);
	return cloned;
}
