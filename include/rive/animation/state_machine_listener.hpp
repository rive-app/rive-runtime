#ifndef _RIVE_STATE_MACHINE_LISTENER_HPP_
#define _RIVE_STATE_MACHINE_LISTENER_HPP_
#include "rive/generated/animation/state_machine_listener_base.hpp"
#include "rive/listener_type.hpp"
#include "rive/math/vec2d.hpp"

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

    ListenerType listenerType() const { return (ListenerType)listenerTypeValue(); }
    size_t actionCount() const { return m_actions.size(); }

    const ListenerAction* action(size_t index) const;
    StatusCode import(ImportStack& importStack) override;

    void performChanges(StateMachineInstance* stateMachineInstance,
                        Vec2D position,
                        Vec2D previousPosition) const;

private:
    void addAction(std::unique_ptr<ListenerAction>);
    std::vector<std::unique_ptr<ListenerAction>> m_actions;
};
} // namespace rive

#endif