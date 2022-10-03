#ifndef _RIVE_BLEND_STATE_DIRECT_BASE_HPP_
#define _RIVE_BLEND_STATE_DIRECT_BASE_HPP_
#include "rive/animation/blend_state.hpp"
namespace rive
{
class BlendStateDirectBase : public BlendState
{
protected:
    typedef BlendState Super;

public:
    static const uint16_t typeKey = 73;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BlendStateDirectBase::typeKey:
            case BlendStateBase::typeKey:
            case LayerStateBase::typeKey:
            case StateMachineLayerComponentBase::typeKey:
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