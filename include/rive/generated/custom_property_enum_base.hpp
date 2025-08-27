#ifndef _RIVE_CUSTOM_PROPERTY_ENUM_BASE_HPP_
#define _RIVE_CUSTOM_PROPERTY_ENUM_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/custom_property.hpp"
namespace rive
{
class CustomPropertyEnumBase : public CustomProperty
{
protected:
    typedef CustomProperty Super;

public:
    static const uint16_t typeKey = 616;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case CustomPropertyEnumBase::typeKey:
            case CustomPropertyBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t propertyValuePropertyKey = 872;
    static const uint16_t enumIdPropertyKey = 873;

protected:
    uint32_t m_PropertyValue = -1;
    uint32_t m_EnumId = -1;

public:
    inline uint32_t propertyValue() const { return m_PropertyValue; }
    void propertyValue(uint32_t value)
    {
        if (m_PropertyValue == value)
        {
            return;
        }
        m_PropertyValue = value;
        propertyValueChanged();
    }

    inline uint32_t enumId() const { return m_EnumId; }
    void enumId(uint32_t value)
    {
        if (m_EnumId == value)
        {
            return;
        }
        m_EnumId = value;
        enumIdChanged();
    }

    Core* clone() const override;
    void copy(const CustomPropertyEnumBase& object)
    {
        m_PropertyValue = object.m_PropertyValue;
        m_EnumId = object.m_EnumId;
        CustomProperty::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case propertyValuePropertyKey:
                m_PropertyValue = CoreUintType::deserialize(reader);
                return true;
            case enumIdPropertyKey:
                m_EnumId = CoreUintType::deserialize(reader);
                return true;
        }
        return CustomProperty::deserialize(propertyKey, reader);
    }

protected:
    virtual void propertyValueChanged() {}
    virtual void enumIdChanged() {}
};
} // namespace rive

#endif