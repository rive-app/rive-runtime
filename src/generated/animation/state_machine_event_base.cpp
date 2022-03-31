#include "rive/generated/animation/state_machine_event_base.hpp"
#include "rive/animation/state_machine_event.hpp"

using namespace rive;

Core* StateMachineEventBase::clone() const {
    auto cloned = new StateMachineEvent();
    cloned->copy(*this);
    return cloned;
}
