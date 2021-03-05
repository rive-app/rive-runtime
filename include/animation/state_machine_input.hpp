#ifndef _RIVE_STATE_MACHINE_INPUT_HPP_
#define _RIVE_STATE_MACHINE_INPUT_HPP_
#include "generated/animation/state_machine_input_base.hpp"
#include <stdio.h>
namespace rive
{
	class StateMachineInput : public StateMachineInputBase
	{
	public:
		StatusCode onAddedDirty(CoreContext* context) override;
		StatusCode onAddedClean(CoreContext* context) override;
	};
} // namespace rive

#endif