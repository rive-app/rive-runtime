#ifndef _RIVE_TEXT_STYLE_AXIS_BASE_HPP_
#define _RIVE_TEXT_STYLE_AXIS_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TextStyleAxisBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 144;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextStyleAxisBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t tagPropertyKey = 289;
    static const uint16_t axisValuePropertyKey = 288;

private:
    uint32_t m_Tag = 0;
    float m_AxisValue = 0.0f;

public:
    inline uint32_t tag() const { return m_Tag; }
    void tag(uint32_t value)
    {
        if (m_Tag == value)
        {
            return;
        }
        m_Tag = value;
        tagChanged();
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
    void copy(const TextStyleAxisBase& object)
    {
        m_Tag = object.m_Tag;
        m_AxisValue = object.m_AxisValue;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case tagPropertyKey:
                m_Tag = CoreUintType::deserialize(reader);
                return true;
            case axisValuePropertyKey:
                m_AxisValue = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void tagChanged() {}
    virtual void axisValueChanged() {}
};
} // namespace rive

#endif