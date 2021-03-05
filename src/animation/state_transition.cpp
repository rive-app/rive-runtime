#include "animation/state_transition.hpp"

using namespace rive;

StatusCode StateTransition::onAddedDirty(CoreContext* context)
{
	return StatusCode::Ok;
}

StatusCode StateTransition::onAddedClean(CoreContext* context)
{
	return StatusCode::Ok;
}