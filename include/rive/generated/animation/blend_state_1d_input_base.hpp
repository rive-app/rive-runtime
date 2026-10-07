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
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/blend_state_1d_input_ext.inl"
#endif
};
} // namespace rive

#endif