#ifndef _RIVE_NESTED_STATE_MACHINE_HPP_
#define _RIVE_NESTED_STATE_MACHINE_HPP_
#include "rive/generated/animation/nested_state_machine_base.hpp"
#include <stdio.h>
namespace rive
{
	class NestedStateMachine : public NestedStateMachineBase
	{
	public:
		void advance(float elapsedSeconds, Artboard* artboard) override;
		void initializeAnimation(Artboard* artboard) override;
	};
} // namespace rive

#endif