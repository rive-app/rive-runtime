#include "generated/animation/state_machine_trigger_base.hpp"
#include "animation/state_machine_trigger.hpp"

using namespace rive;

Core* StateMachineTriggerBase::clone() const
{
	auto cloned = new StateMachineTrigger();
	cloned->copy(*this);
	return cloned;
}
