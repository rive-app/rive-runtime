#ifndef _RIVE_STATE_MACHINE_EVENT_HPP_
#define _RIVE_STATE_MACHINE_EVENT_HPP_
#include "rive/generated/animation/state_machine_event_base.hpp"
#include <stdio.h>
namespace rive {
    class StateMachineEvent : public StateMachineEventBase {
    public:
        StatusCode import(ImportStack& importStack) override;
    };
} // namespace rive

#endif