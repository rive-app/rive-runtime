#ifndef _RIVE_GRADIENT_STOP_BASE_HPP_
#define _RIVE_GRADIENT_STOP_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_color_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class GradientStopBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 19;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case GradientStopBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t colorValuePropertyKey = 38;
    static const uint16_t positionPropertyKey = 39;

private:
    int m_ColorValue = 0xFFFFFFFF;
    float m_Position = 0.0f;

public:
    inline int colorValue() const { return m_ColorValue; }
    void colorValue(int value)
    {
        if (m_ColorValue == value)
        {
            return;
        }
        m_ColorValue = value;
        colorValueChanged();
    }

    inline float position() const { return m_Position; }
    void position(float value)
    {
        if (m_Position == value)
        {
            return;
        }
        m_Position = value;
        positionChanged();
    }

    Core* clone() const override;
    void copy(const GradientStopBase& object)
    {
        m_ColorValue = object.m_ColorValue;
        m_Position = object.m_Position;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case colorValuePropertyKey:
                m_ColorValue = CoreColorType::deserialize(reader);
                return true;
            case positionPropertyKey:
                m_Position = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void colorValueChanged() {}
    virtual void positionChanged() {}
};
} // namespace rive

#endif