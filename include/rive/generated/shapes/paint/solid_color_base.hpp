#ifndef _RIVE_SOLID_COLOR_BASE_HPP_
#define _RIVE_SOLID_COLOR_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_color_type.hpp"
namespace rive
{
class SolidColorBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 18;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case SolidColorBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t colorValuePropertyKey = 37;

private:
    int m_ColorValue = 0xFF747474;

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

    Core* clone() const override;
    void copy(const SolidColorBase& object)
    {
        m_ColorValue = object.m_ColorValue;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case colorValuePropertyKey:
                m_ColorValue = CoreColorType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void colorValueChanged() {}
};
} // namespace rive

#endif