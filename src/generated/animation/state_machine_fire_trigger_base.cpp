#include "rive/generated/animation/state_machine_fire_trigger_base.hpp"
#include "rive/animation/state_machine_fire_trigger.hpp"

using namespace rive;

Core* StateMachineFireTriggerBase::clone() const
{
    auto cloned = new StateMachineFireTrigger();
    cloned->copy(*this);
    return cloned;
}
