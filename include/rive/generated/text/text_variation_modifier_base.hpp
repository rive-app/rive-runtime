#ifndef _RIVE_TEXT_VARIATION_MODIFIER_BASE_HPP_
#define _RIVE_TEXT_VARIATION_MODIFIER_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/text/text_shape_modifier.hpp"
namespace rive
{
class TextVariationModifierBase : public TextShapeModifier
{
protected:
    typedef TextShapeModifier Super;

public:
    static const uint16_t typeKey = 162;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextVariationModifierBase::typeKey:
            case TextShapeModifierBase::typeKey:
            case TextModifierBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t axisTagPropertyKey = 320;
    static const uint16_t axisValuePropertyKey = 321;

private:
    uint32_t m_AxisTag = 0;
    float m_AxisValue = 0.0f;

public:
    inline uint32_t axisTag() const { return m_AxisTag; }
    void axisTag(uint32_t value)
    {
        if (m_AxisTag == value)
        {
            return;
        }
        m_AxisTag = value;
        axisTagChanged();
    }

    inline float axisValue() const { return m_AxisValue; }
    void axisValue(float value)
    {
        if (m_AxisValue == value)
        {
            return;
        }
        m_AxisValue = value;
        axisValueChanged();
    }

    Core* clone() const override;
    void copy(const TextVariationModifierBase& object)
    {
        m_AxisTag = object.m_AxisTag;
        m_AxisValue = object.m_AxisValue;
        TextShapeModifier::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case axisTagPropertyKey:
                m_AxisTag = CoreUintType::deserialize(reader);
                return true;
            case axisValuePropertyKey:
                m_AxisValue = CoreDoubleType::deserialize(reader);
                return true;
        }
        return TextShapeModifier::deserialize(propertyKey, reader);
    }

protected:
    virtual void axisTagChanged() {}
    virtual void axisValueChanged() {}
};
} // namespace rive

#endif