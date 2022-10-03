#ifndef _RIVE_LAYER_STATE_BASE_HPP_
#define _RIVE_LAYER_STATE_BASE_HPP_
#include "rive/animation/state_machine_layer_component.hpp"
namespace rive
{
class LayerStateBase : public StateMachineLayerComponent
{
protected:
    typedef StateMachineLayerComponent Super;

public:
    static const uint16_t typeKey = 60;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case LayerStateBase::typeKey:
            case StateMachineLayerComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

protected:
};
} // namespace rive

#endif