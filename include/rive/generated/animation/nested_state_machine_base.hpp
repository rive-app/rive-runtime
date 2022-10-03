#ifndef _RIVE_NESTED_STATE_MACHINE_BASE_HPP_
#define _RIVE_NESTED_STATE_MACHINE_BASE_HPP_
#include "rive/nested_animation.hpp"
namespace rive
{
class NestedStateMachineBase : public NestedAnimation
{
protected:
    typedef NestedAnimation Super;

public:
    static const uint16_t typeKey = 95;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NestedStateMachineBase::typeKey:
            case NestedAnimationBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
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