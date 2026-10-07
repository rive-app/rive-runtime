#ifndef _RIVE_BLEND_ANIMATION_DIRECT_BASE_HPP_
#define _RIVE_BLEND_ANIMATION_DIRECT_BASE_HPP_
#include "rive/animation/blend_animation.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class BlendAnimationDirectBase : public BlendAnimation
{
protected:
    typedef BlendAnimation Super;

public:
    static const uint16_t typeKey = 77;

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t inputIdPropertyKey = 168;
    static const uint16_t mixValuePropertyKey = 297;
    static const uint16_t blendSourcePropertyKey = 298;

protected:
    Id m_InputId = kEmptyId;
    float m_MixValue = 100.0f;
    uint32_t m_BlendSource = 0;

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

    inline float mixValue() const { return m_MixValue; }
    void mixValue(float value)
    {
        if (m_MixValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(mixValuePropertyKey, &m_MixValue, &value);
        m_MixValue = value;
        notifyPropertyChanged(mixValuePropertyKey);
    }

    inline uint32_t blendSource() const { return m_BlendSource; }
    void blendSource(uint32_t value)
    {
        if (m_BlendSource == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(blendSourcePropertyKey, &m_BlendSource, &value);
        m_BlendSource = value;
        notifyPropertyChanged(blendSourcePropertyKey);
    }

    Core* clone() const override;
    void copy(const BlendAnimationDirectBase& object)
    {
        m_InputId = object.m_InputId;
        m_MixValue = object.m_MixValue;
        m_BlendSource = object.m_BlendSource;
        RIVE_EDITOR_COPY(object);
        BlendAnimation::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case inputIdPropertyKey:
                m_InputId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case mixValuePropertyKey:
                m_MixValue = CoreDoubleType::deserialize(reader);
                return true;
            case blendSourcePropertyKey:
                m_BlendSource = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return BlendAnimation::deserialize(propertyKey, reader);
    }

protected:
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/blend_animation_direct_ext.inl"
#endif
};
} // namespace rive

#endif