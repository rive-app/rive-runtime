#ifndef _RIVE_CUSTOM_PROPERTY_BASE_HPP_
#define _RIVE_CUSTOM_PROPERTY_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class CustomPropertyBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 167;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case CustomPropertyBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t nameIdPropertyKey = 449;

protected:
    uint32_t m_NameId = -1;

public:
    inline uint32_t nameId() const { return m_NameId; }
    void nameId(uint32_t value)
    {
        if (m_NameId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(nameIdPropertyKey, &m_NameId, &value);
        m_NameId = value;
        RIVE_EDITOR_CHANGED(nameIdChanged());
        notifyPropertyChanged(nameIdPropertyKey);
    }

    void copy(const CustomPropertyBase& object)
    {
        m_NameId = object.m_NameId;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case nameIdPropertyKey:
                m_NameId = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void nameIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/custom_property_ext.inl"
#endif
};
} // namespace rive

#endif