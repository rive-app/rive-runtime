#include "rive/generated/animation/state_machine_listener_single_base.hpp"
#include "rive/animation/state_machine_listener_single.hpp"

using namespace rive;

Core* StateMachineListenerSingleBase::clone() const
{
    auto cloned = new StateMachineListenerSingle();
    cloned->copy(*this);
    return cloned;
}
