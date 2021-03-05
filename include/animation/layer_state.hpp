#ifndef _RIVE_LAYER_STATE_HPP_
#define _RIVE_LAYER_STATE_HPP_
#include "generated/animation/layer_state_base.hpp"
#include <stdio.h>
namespace rive
{
	class LayerState : public LayerStateBase
	{
	public:
		StatusCode onAddedDirty(CoreContext* context) override;
		StatusCode onAddedClean(CoreContext* context) override;
	};
} // namespace rive

#endif