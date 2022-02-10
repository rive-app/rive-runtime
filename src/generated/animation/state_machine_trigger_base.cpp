#include "rive/generated/animation/state_machine_trigger_base.hpp"
#include "rive/animation/state_machine_trigger.hpp"

using namespace rive;

Core* StateMachineTriggerBase::clone() const
{
    auto cloned = new StateMachineTrigger();
    cloned->copy(*this);
    return cloned;
}
