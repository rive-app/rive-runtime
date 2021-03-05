#include "animation/layer_state.hpp"
using namespace rive;
#include "animation/transition_bool_condition.hpp"

using namespace rive;

StatusCode LayerState::onAddedDirty(CoreContext* context)
{
	return StatusCode::Ok;
}

StatusCode LayerState::onAddedClean(CoreContext* context)
{
	return StatusCode::Ok;
}