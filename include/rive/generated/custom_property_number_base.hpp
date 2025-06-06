#ifndef _RIVE_CUSTOM_PROPERTY_NUMBER_BASE_HPP_
#define _RIVE_CUSTOM_PROPERTY_NUMBER_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/custom_property.hpp"
namespace rive
{
class CustomPropertyNumberBase : public CustomProperty
{
protected:
    typedef CustomProperty Super;

public:
    static const uint16_t typeKey = 127;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case CustomPropertyNumberBase::typeKey:
            case CustomPropertyBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t propertyValuePropertyKey = 243;

protected:
    float m_PropertyValue = 0.0f;

public:
    inline float propertyValue() const { return m_PropertyValue; }
    void propertyValue(float value)
    {
        if (m_PropertyValue == value)
        {
            return;
        }
        m_PropertyValue = value;
        propertyValueChanged();
    }

    Core* clone() const override;
    void copy(const CustomPropertyNumberBase& object)
    {
        m_PropertyValue = object.m_PropertyValue;
        CustomProperty::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case propertyValuePropertyKey:
                m_PropertyValue = CoreDoubleType::deserialize(reader);
                return true;
        }
        return CustomProperty::deserialize(propertyKey, reader);
    }

protected:
    virtual void propertyValueChanged() {}
};
} // namespace rive

#endif