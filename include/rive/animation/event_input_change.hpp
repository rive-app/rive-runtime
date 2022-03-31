#ifndef _RIVE_EVENT_INPUT_CHANGE_HPP_
#define _RIVE_EVENT_INPUT_CHANGE_HPP_
#include "rive/generated/animation/event_input_change_base.hpp"

namespace rive {
    class EventInputChange : public EventInputChangeBase {
    public:
        StatusCode import(ImportStack& importStack) override;
    };
} // namespace rive

#endif