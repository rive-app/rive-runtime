#ifndef _RIVE_CUSTOM_PROPERTY_STRING_BASE_HPP_
#define _RIVE_CUSTOM_PROPERTY_STRING_BASE_HPP_
#include <string>
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/custom_property.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class CustomPropertyStringBase : public CustomProperty
{
protected:
    typedef CustomProperty Super;

public:
    static const uint16_t typeKey = 130;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case CustomPropertyStringBase::typeKey:
            case CustomPropertyBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t propertyValuePropertyKey = 246;

protected:
    std::string m_PropertyValue = "";

public:
    inline const std::string& propertyValue() const { return m_PropertyValue; }
    void propertyValue(std::string value)
    {
        if (m_PropertyValue == value)
        {
            return;
        }
        RIVE_EDITOR_STRING_CHANGING(propertyValuePropertyKey,
                                    m_PropertyValue,
                                    value);
        m_PropertyValue = value;
        RIVE_EDITOR_CHANGED(propertyValueChanged());
        notifyPropertyChanged(propertyValuePropertyKey);
    }

    Core* clone() const override;
    void copy(const CustomPropertyStringBase& object)
    {
        m_PropertyValue = object.m_PropertyValue;
        RIVE_EDITOR_COPY(object);
        CustomProperty::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case propertyValuePropertyKey:
                m_PropertyValue = CoreStringType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return CustomProperty::deserialize(propertyKey, reader);
    }

protected:
    virtual void propertyValueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/custom_property_string_ext.inl"
#endif
};
} // namespace rive

#endif