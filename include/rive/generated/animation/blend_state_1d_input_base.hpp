#ifndef _RIVE_BLEND_STATE1_DINPUT_BASE_HPP_
#define _RIVE_BLEND_STATE1_DINPUT_BASE_HPP_
#include "rive/animation/blend_state_1d.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class BlendState1DInputBase : public BlendState1D
{
protected:
    typedef BlendState1D Super;

public:
    static const uint16_t typeKey = 76;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BlendState1DInputBase::typeKey:
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

    static const uint16_t inputIdPropertyKey = 167;

protected:
    Id m_InputId = kEmptyId;

public:
    inline Id inputId() const { return m_InputId; }
    void inputId(Id value)
    {
        if (m_InputId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(inputIdPropertyKey, &m_InputId, &value);
        m_InputId = value;
        RIVE_EDITOR_CHANGED(inputIdChanged());
        notifyPropertyChanged(inputIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const BlendState1DInputBase& object)
    {
        m_InputId = object.m_InputId;
        BlendState1D::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case inputIdPropertyKey:
                m_InputId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return BlendState1D::deserialize(propertyKey, reader);
    }

protected:
    virtual void inputIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/blend_state_1d_input_ext.inl"
#endif
};
} // namespace rive

#endif