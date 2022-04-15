#ifndef _RIVE_EVENT_INPUT_CHANGE_HPP_
#define _RIVE_EVENT_INPUT_CHANGE_HPP_
#include "rive/generated/animation/event_input_change_base.hpp"

namespace rive {
    class StateMachineInstance;
    class StateMachineInput;
    class EventInputChange : public EventInputChangeBase {
    public:
        StatusCode import(ImportStack& importStack) override;
        virtual void perform(StateMachineInstance* stateMachineInstance) const = 0;
        virtual bool validateInputType(const StateMachineInput* input) const { return true; }
    };
} // namespace rive

#endif