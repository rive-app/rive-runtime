#include "generated/animation/transition_number_condition_base.hpp"
#include "animation/transition_number_condition.hpp"

using namespace rive;

Core* TransitionNumberConditionBase::clone() const
{
	auto cloned = new TransitionNumberCondition();
	cloned->copy(*this);
	return cloned;
}
