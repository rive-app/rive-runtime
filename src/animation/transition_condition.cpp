#include "animation/transition_bool_condition.hpp"

using namespace rive;

StatusCode TransitionCondition::onAddedDirty(CoreContext* context)
{
	return StatusCode::Ok;
}

StatusCode TransitionCondition::onAddedClean(CoreContext* context)
{
	return StatusCode::Ok;
}