#ifndef _RIVE_STATE_MACHINE_TRIGGER_HPP_
#define _RIVE_STATE_MACHINE_TRIGGER_HPP_
#include "generated/animation/state_machine_trigger_base.hpp"
#include <stdio.h>
namespace rive
{
	class StateMachineTrigger : public StateMachineTriggerBase
	{
	private: 
		bool m_Fired = false;
		void reset();

	public:
		void fire();
	};
} // namespace rive

#endif