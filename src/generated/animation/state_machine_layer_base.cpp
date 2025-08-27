#include "rive/generated/animation/state_machine_layer_base.hpp"
#include "rive/animation/state_machine_layer.hpp"

using namespace rive;

Core* StateMachineLayerBase::clone() const
{
    auto cloned = new StateMachineLayer();
    cloned->copy(*this);
    return cloned;
}
