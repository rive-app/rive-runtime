#ifndef _RIVE_BLEND_STATE1_DVIEW_MODEL_BASE_HPP_
#define _RIVE_BLEND_STATE1_DVIEW_MODEL_BASE_HPP_
#include "rive/animation/blend_state_1d.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
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

public:
    Core* clone() const override;
    void copy(const BlendState1DViewModelBase& object)
    {
        RIVE_EDITOR_COPY(object);
        BlendState1D::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return BlendState1D::deserialize(propertyKey, reader);
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/blend_state_1d_viewmodel_ext.inl"
#endif
};
} // namespace rive

#endif