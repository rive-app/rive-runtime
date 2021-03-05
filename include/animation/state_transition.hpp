#ifndef _RIVE_STATE_TRANSITION_HPP_
#define _RIVE_STATE_TRANSITION_HPP_
#include "generated/animation/state_transition_base.hpp"
#include <stdio.h>
namespace rive
{
	class StateTransition : public StateTransitionBase
	{
	public:
		StatusCode onAddedDirty(CoreContext* context) override;
		StatusCode onAddedClean(CoreContext* context) override;
	};
} // namespace rive

#endif