#ifndef _RIVE_BLEND_STATE1_DVIEW_MODEL_BASE_HPP_
#define _RIVE_BLEND_STATE1_DVIEW_MODEL_BASE_HPP_
#include "rive/animation/blend_state_1d.hpp"
namespace rive
{
class BlendState1DViewModelBase : public BlendState1D
{
protected:
    typedef BlendState1D Super;

public:
    static const uint16_t typeKey = 528;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BlendState1DViewModelBase::typeKey:
            case BlendState1DBase::typeKey:
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