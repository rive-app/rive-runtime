#ifndef _RIVE_EVENT_TRIGGER_CHANGE_HPP_
#define _RIVE_EVENT_TRIGGER_CHANGE_HPP_
#include "rive/generated/animation/event_trigger_change_base.hpp"

namespace rive {
    class EventTriggerChange : public EventTriggerChangeBase {
    public:
        bool validateInputType(const StateMachineInput* input) const override;
        void perform(StateMachineInstance* stateMachineInstance) const override;
    };
} // namespace rive

#endif