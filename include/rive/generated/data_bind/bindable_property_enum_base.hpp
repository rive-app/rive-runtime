#ifndef _RIVE_BINDABLE_PROPERTY_ENUM_BASE_HPP_
#define _RIVE_BINDABLE_PROPERTY_ENUM_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/data_bind/bindable_property.hpp"
namespace rive
{
class BindablePropertyEnumBase : public BindableProperty
{
protected:
    typedef BindableProperty Super;

public:
    static const uint16_t typeKey = 474;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BindablePropertyEnumBase::typeKey:
            case BindablePropertyBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t propertyValuePropertyKey = 637;

protected:
    Id m_PropertyValue = kEmptyId;

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

    Core* clone() const override;
    void copy(const BindablePropertyEnumBase& object)
    {
        m_PropertyValue = object.m_PropertyValue;
        BindableProperty::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case propertyValuePropertyKey:
                m_PropertyValue = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return BindableProperty::deserialize(propertyKey, reader);
    }

protected:
    virtual void propertyValueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/data_bind/bindable_property_enum_ext.inl"
#endif
};
} // namespace rive

#endif