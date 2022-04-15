#ifndef _RIVE_EVENT_NUMBER_CHANGE_HPP_
#define _RIVE_EVENT_NUMBER_CHANGE_HPP_
#include "rive/generated/animation/event_number_change_base.hpp"

namespace rive {
    class EventNumberChange : public EventNumberChangeBase {
    public:
        bool validateInputType(const StateMachineInput* input) const override;
        void perform(StateMachineInstance* stateMachineInstance) const override;
    };
} // namespace rive

#endif