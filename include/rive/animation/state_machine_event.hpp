#ifndef _RIVE_STATE_MACHINE_EVENT_HPP_
#define _RIVE_STATE_MACHINE_EVENT_HPP_
#include "rive/generated/animation/state_machine_event_base.hpp"

namespace rive {
    class StateMachineEventImporter;
    class EventInputChange;
    class StateMachineEvent : public StateMachineEventBase {
        friend class StateMachineEventImporter;

    private:
        std::vector<EventInputChange*> m_InputChanges;
        void addInputChange(EventInputChange* inputChange);

    public:
        size_t inputChangeCount() const { return m_InputChanges.size(); }
        const EventInputChange* inputChange(size_t index) const;
        StatusCode import(ImportStack& importStack) override;
    };
} // namespace rive

#endif