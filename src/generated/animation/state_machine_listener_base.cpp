#include "rive/generated/animation/state_machine_listener_base.hpp"
#include "rive/animation/state_machine_listener.hpp"

using namespace rive;

Core* StateMachineListenerBase::clone() const
{
    auto cloned = new StateMachineListener();
    cloned->copy(*this);
    return cloned;
}
