#include "rive/generated/animation/nested_state_machine_base.hpp"
#include "rive/animation/nested_state_machine.hpp"

using namespace rive;

Core* NestedStateMachineBase::clone() const
{
    auto cloned = new NestedStateMachine();
    cloned->copy(*this);
    return cloned;
}
