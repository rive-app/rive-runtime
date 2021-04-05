#ifndef _RIVE_STATE_MACHINE_HPP_
#define _RIVE_STATE_MACHINE_HPP_
#include "generated/animation/state_machine_base.hpp"
#include <stdio.h>
namespace rive
{
	class StateMachine : public StateMachineBase
	{
	public:
		StatusCode import(ImportStack& importStack) override;
	};
} // namespace rive

#endif