#ifndef _RIVE_STATE_MACHINE_LISTENER_HPP_
#define _RIVE_STATE_MACHINE_LISTENER_HPP_
#include "rive/animation/listener_invocation.hpp"
#include "rive/generated/animation/state_machine_listener_base.hpp"
#include "rive/listener_type.hpp"
#include "rive/span.hpp"
#include "rive/animation/listener_types/listener_input_type.hpp"

namespace rive
{
class Shape;
class StateMachineListenerImporter;
class ListenerAction;
class StateMachineInstance;
class StateMachineListener : public StateMachineListenerBase
{
    friend class StateMachineListenerImporter;

public:
    StateMachineListener();
    ~StateMachineListener() override;

    // ListenerType listenerType() const
    // {
    //     return (ListenerType)listenerTypeValue();
    // }
    virtual bool hasListener(ListenerType) const;
    bool hasListeners(Span<const ListenerType> listenerTypes) const;
    size_t actionCount() const { return m_actions.size(); }
    size_t listenerInputTypeCount() const
    {
        return m_listenerInputTypes.size();
    }

    const ListenerAction* action(size_t index) const;
    const ListenerInputType* listenerInputType(size_t index) const;
    StatusCode import(ImportStack& importStack) override;

    void performChanges(StateMachineInstance* stateMachineInstance,
                        const ListenerInvocation& invocation) const;

private:
    void addAction(std::unique_ptr<ListenerAction>);
    void addListenerInputType(std::unique_ptr<ListenerInputType>);
    std::vector<std::unique_ptr<ListenerAction>> m_actions;
    std::vector<std::unique_ptr<ListenerInputType>> m_listenerInputTypes;
};
} // namespace rive

#endif