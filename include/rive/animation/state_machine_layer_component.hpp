#ifndef _RIVE_STATE_MACHINE_LAYER_COMPONENT_HPP_
#define _RIVE_STATE_MACHINE_LAYER_COMPONENT_HPP_
#include "rive/generated/animation/state_machine_layer_component_base.hpp"
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
    ~StateMachineLayerComponent() override;

private:
    std::vector<StateMachineFireAction*> m_events;
};
} // namespace rive

#endif