#ifndef _RIVE_BLEND_ANIMATION_DIRECT_BASE_HPP_
#define _RIVE_BLEND_ANIMATION_DIRECT_BASE_HPP_
#include "rive/animation/blend_animation.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class BlendAnimationDirectBase : public BlendAnimation
{
protected:
    typedef BlendAnimation Super;

public:
    static const uint16_t typeKey = 77;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BlendAnimationDirectBase::typeKey:
            case BlendAnimationBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t inputIdPropertyKey = 168;
    static const uint16_t mixValuePropertyKey = 297;
    static const uint16_t blendSourcePropertyKey = 298;

private:
    uint32_t m_InputId = -1;
    float m_MixValue = 100.0f;
    uint32_t m_BlendSource = 0;

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

    inline float mixValue() const { return m_MixValue; }
    void mixValue(float value)
    {
        if (m_MixValue == value)
        {
            return;
        }
        m_MixValue = value;
        mixValueChanged();
    }

    inline uint32_t blendSource() const { return m_BlendSource; }
    void blendSource(uint32_t value)
    {
        if (m_BlendSource == value)
        {
            return;
        }
        m_BlendSource = value;
        blendSourceChanged();
    }

    Core* clone() const override;
    void copy(const BlendAnimationDirectBase& object)
    {
        m_InputId = object.m_InputId;
        m_MixValue = object.m_MixValue;
        m_BlendSource = object.m_BlendSource;
        BlendAnimation::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case inputIdPropertyKey:
                m_InputId = CoreUintType::deserialize(reader);
                return true;
            case mixValuePropertyKey:
                m_MixValue = CoreDoubleType::deserialize(reader);
                return true;
            case blendSourcePropertyKey:
                m_BlendSource = CoreUintType::deserialize(reader);
                return true;
        }
        return BlendAnimation::deserialize(propertyKey, reader);
    }

protected:
    virtual void inputIdChanged() {}
    virtual void mixValueChanged() {}
    virtual void blendSourceChanged() {}
};
} // namespace rive

#endif