#ifndef _RIVE_BLEND_STATE1_DINPUT_BASE_HPP_
#define _RIVE_BLEND_STATE1_DINPUT_BASE_HPP_
#include "rive/animation/blend_state_1d.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
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
    uint32_t m_InputId = -1;

public:
    inline uint32_t inputId() const { return m_InputId; }
    void inputId(uint32_t value)
    {
        if (m_InputId == value)
        {
            return;
        }
        m_InputId = value;
        inputIdChanged();
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
                m_InputId = CoreUintType::deserialize(reader);
                return true;
        }
        return BlendState1D::deserialize(propertyKey, reader);
    }

protected:
    virtual void inputIdChanged() {}
};
} // namespace rive

#endif