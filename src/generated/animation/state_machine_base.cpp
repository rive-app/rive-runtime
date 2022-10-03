#include "rive/generated/animation/state_machine_base.hpp"
#include "rive/animation/state_machine.hpp"

using namespace rive;

Core* StateMachineBase::clone() const
{
    auto cloned = new StateMachine();
    cloned->copy(*this);
    return cloned;
}
