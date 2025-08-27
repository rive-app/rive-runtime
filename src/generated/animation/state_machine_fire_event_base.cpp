#include "rive/generated/animation/state_machine_fire_event_base.hpp"
#include "rive/animation/state_machine_fire_event.hpp"

using namespace rive;

Core* StateMachineFireEventBase::clone() const
{
    auto cloned = new StateMachineFireEvent();
    cloned->copy(*this);
    return cloned;
}
