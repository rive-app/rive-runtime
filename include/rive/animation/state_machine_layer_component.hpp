#ifndef _RIVE_STATE_MACHINE_LAYER_COMPONENT_HPP_
#define _RIVE_STATE_MACHINE_LAYER_COMPONENT_HPP_
#include "rive/generated/animation/state_machine_layer_component_base.hpp"
#include <vector>

namespace rive
{
class StateMachineFireEvent;
class StateMachineLayerComponent : public StateMachineLayerComponentBase
{
    friend class StateMachineLayerComponentImporter;

public:
    const std::vector<StateMachineFireEvent*>& events() const { return m_events; }
    ~StateMachineLayerComponent() override;

private:
    std::vector<StateMachineFireEvent*> m_events;
};
} // namespace rive

#endif