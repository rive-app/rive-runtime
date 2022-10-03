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

private:
    std::vector<uint32_t> m_HitShapesIds;
    std::vector<std::unique_ptr<ListenerAction>> m_Actions;
    void addAction(std::unique_ptr<ListenerAction>);

public:
    StateMachineListener();
    ~StateMachineListener() override;

    ListenerType listenerType() const { return (ListenerType)listenerTypeValue(); }
    size_t actionCount() const { return m_Actions.size(); }

    const ListenerAction* action(size_t index) const;
    StatusCode import(ImportStack& importStack) override;
    StatusCode onAddedClean(CoreContext* context) override;

    const std::vector<uint32_t>& hitShapeIds() const { return m_HitShapesIds; }
    void performChanges(StateMachineInstance* stateMachineInstance, Vec2D position) const;
};
} // namespace rive

#endif