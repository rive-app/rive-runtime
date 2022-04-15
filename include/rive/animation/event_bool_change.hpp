#ifndef _RIVE_EVENT_BOOL_CHANGE_HPP_
#define _RIVE_EVENT_BOOL_CHANGE_HPP_
#include "rive/generated/animation/event_bool_change_base.hpp"

namespace rive {
    class EventBoolChange : public EventBoolChangeBase {
    public:
        bool validateInputType(const StateMachineInput* input) const override;
        void perform(StateMachineInstance* stateMachineInstance) const override;
    };
} // namespace rive

#endif