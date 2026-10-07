#ifndef _RIVE_CUSTOM_PROPERTY_ENUM_BASE_HPP_
#define _RIVE_CUSTOM_PROPERTY_ENUM_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/custom_property.hpp"
namespace rive
{
class CustomPropertyEnumBase : public CustomProperty
{
protected:
    typedef CustomProperty Super;

public:
    static const uint16_t typeKey = 616;

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t propertyValuePropertyKey = 872;
    static const uint16_t enumIdPropertyKey = 873;

protected:
    Id m_PropertyValue = kEmptyId;
    Id m_EnumId = kEmptyId;

public:
    inline Id propertyValue() const { return m_PropertyValue; }
    void propertyValue(Id value)
    {
        if (m_PropertyValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(propertyValuePropertyKey,
                             &m_PropertyValue,
                             &value);
        m_PropertyValue = value;
        RIVE_EDITOR_CHANGED(propertyValueChanged());
        notifyPropertyChanged(propertyValuePropertyKey);
    }

    inline Id enumId() const { return m_EnumId; }
    void enumId(Id value)
    {
        if (m_EnumId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(enumIdPropertyKey, &m_EnumId, &value);
        m_EnumId = value;
        notifyPropertyChanged(enumIdPropertyKey);
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
                m_PropertyValue = CoreIdType::runtimeDeserialize(reader);
                return true;
            case enumIdPropertyKey:
                m_EnumId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return CustomProperty::deserialize(propertyKey, reader);
    }

protected:
    virtual void propertyValueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/custom_property_enum_ext.inl"
#endif
};
} // namespace rive

#endif