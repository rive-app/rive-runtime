#include "generated/animation/state_transition_base.hpp"
#include "animation/state_transition.hpp"

using namespace rive;

Core* StateTransitionBase::clone() const
{
	auto cloned = new StateTransition();
	cloned->copy(*this);
	return cloned;
}
