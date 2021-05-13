#include "artboard.hpp"
#include "animation/blend_state_transition.hpp"

using namespace rive;

StatusCode BlendStateTransition::onAddedClean(CoreContext* context)
{
	// 	auto id = exitBlendAnimationId();
	// 	if (id >= 0)
	// 	{
	// 		auto artboard = static_cast<Artboard*>(context);
	// 		if (artboard->animationCount() < id)
	// 		{
	// 			m_ExitBlendAnimation = artboard->animation(id);
	// 		}
	// 	}
	printf("BLEND STATE TRANSITION ADDED CLEAN %i\n", exitBlendAnimationId());
	return StatusCode::Ok;
}