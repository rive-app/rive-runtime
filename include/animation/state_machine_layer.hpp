#ifndef _RIVE_STATE_MACHINE_LAYER_HPP_
#define _RIVE_STATE_MACHINE_LAYER_HPP_
#include "generated/animation/state_machine_layer_base.hpp"
#include <stdio.h>
namespace rive
{
	class StateMachineLayer : public StateMachineLayerBase
	{
	public:
		StatusCode onAddedDirty(CoreContext* context) override;
		StatusCode onAddedClean(CoreContext* context) override;
	};
} // namespace rive

#endif