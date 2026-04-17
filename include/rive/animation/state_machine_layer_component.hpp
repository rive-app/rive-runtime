#ifndef _RIVE_STATE_MACHINE_LAYER_COMPONENT_HPP_
#define _RIVE_STATE_MACHINE_LAYER_COMPONENT_HPP_
#include "rive/animation/listener_action.hpp"
#include "rive/generated/animation/state_machine_layer_component_base.hpp"
#include <memory>
#include <vector>

namespace rive
{
class StateMachineFireAction;
class StateMachineLayerComponent : public StateMachineLayerComponentBase
{
    friend class StateMachineLayerComponentImporter;

public:
    const std::vector<StateMachineFireAction*>& events() const
    {
        return m_events;
    }
    const std::vector<std::unique_ptr<ListenerAction>>& listenerActions() const
    {
        return m_listenerActions;
    }
    ~StateMachineLayerComponent() override;

private:
    std::vector<StateMachineFireAction*> m_events;
    std::vector<std::unique_ptr<ListenerAction>> m_listenerActions;
};
} // namespace rive

#endif