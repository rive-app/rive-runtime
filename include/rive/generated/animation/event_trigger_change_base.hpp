#ifndef _RIVE_EVENT_TRIGGER_CHANGE_BASE_HPP_
#define _RIVE_EVENT_TRIGGER_CHANGE_BASE_HPP_
#include "rive/animation/event_input_change.hpp"
namespace rive {
    class EventTriggerChangeBase : public EventInputChange {
    protected:
        typedef EventInputChange Super;

    public:
        static const uint16_t typeKey = 115;

        /// Helper to quickly determine if a core object extends another without RTTI
        /// at runtime.
        bool isTypeOf(uint16_t typeKey) const override {
            switch (typeKey) {
                case EventTriggerChangeBase::typeKey:
                case EventInputChangeBase::typeKey:
                    return true;
                default:
                    return false;
            }
        }

        uint16_t coreType() const override { return typeKey; }

        Core* clone() const override;

    protected:
    };
} // namespace rive

#endif