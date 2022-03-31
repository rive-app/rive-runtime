#ifndef _RIVE_STATE_MACHINE_EVENT_IMPORTER_HPP_
#define _RIVE_STATE_MACHINE_EVENT_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive {
    class StateMachineEvent;
    class StateMachine;
    class EventInputChange;
    class StateMachineEventImporter : public ImportStackObject {
    private:
        StateMachineEvent* m_StateMachineEvent;

    public:
        StateMachineEventImporter(StateMachineEvent* event);
        const StateMachineEvent* stateMachineEvent() const { return m_StateMachineEvent; }
        void addInputChange(EventInputChange* change);
        StatusCode resolve() override;
    };
} // namespace rive
#endif
