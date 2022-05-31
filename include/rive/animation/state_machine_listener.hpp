#ifndef _RIVE_STATE_MACHINE_LISTENER_HPP_
#define _RIVE_STATE_MACHINE_LISTENER_HPP_
#include "rive/generated/animation/state_machine_listener_base.hpp"
#include "rive/listener_type.hpp"

namespace rive {
    class Shape;
    class StateMachineListenerImporter;
    class ListenerInputChange;
    class StateMachineInstance;
    class StateMachineListener : public StateMachineListenerBase {
        friend class StateMachineListenerImporter;

    private:
        std::vector<uint32_t> m_HitShapesIds;
        std::vector<ListenerInputChange*> m_InputChanges;
        void addInputChange(ListenerInputChange* inputChange);

    public:
        ListenerType listenerType() const { return (ListenerType)listenerTypeValue(); }
        size_t inputChangeCount() const { return m_InputChanges.size(); }
        const ListenerInputChange* inputChange(size_t index) const;
        StatusCode import(ImportStack& importStack) override;
        StatusCode onAddedClean(CoreContext* context) override;

        const std::vector<uint32_t>& hitShapeIds() const { return m_HitShapesIds; }
        void performChanges(StateMachineInstance* stateMachineInstance) const;
    };
} // namespace rive

#endif