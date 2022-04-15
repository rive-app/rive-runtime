#ifndef _RIVE_STATE_MACHINE_EVENT_HPP_
#define _RIVE_STATE_MACHINE_EVENT_HPP_
#include "rive/generated/animation/state_machine_event_base.hpp"
#include "rive/event_type.hpp"

namespace rive {
    class Shape;
    class StateMachineEventImporter;
    class EventInputChange;
    class StateMachineInstance;
    class StateMachineEvent : public StateMachineEventBase {
        friend class StateMachineEventImporter;

    private:
        std::vector<uint32_t> m_HitShapesIds;
        std::vector<EventInputChange*> m_InputChanges;
        void addInputChange(EventInputChange* inputChange);

    public:
        EventType eventType() const { return (EventType)eventTypeValue(); }
        size_t inputChangeCount() const { return m_InputChanges.size(); }
        const EventInputChange* inputChange(size_t index) const;
        StatusCode import(ImportStack& importStack) override;
        StatusCode onAddedClean(CoreContext* context) override;

        const std::vector<uint32_t>& hitShapeIds() const { return m_HitShapesIds; }
        void performChanges(StateMachineInstance* stateMachineInstance) const;
    };
} // namespace rive

#endif