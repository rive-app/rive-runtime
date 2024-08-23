#ifndef _RIVE_STATE_MACHINE_LAYER_BASE_HPP_
#define _RIVE_STATE_MACHINE_LAYER_BASE_HPP_
#include "rive/animation/state_machine_component.hpp"
namespace rive
{
class StateMachineLayerBase : public StateMachineComponent
{
protected:
    typedef StateMachineComponent Super;

public:
    static const uint16_t typeKey = 57;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StateMachineLayerBase::typeKey:
            case StateMachineComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;

protected:
};
} // namespace rive

#endif