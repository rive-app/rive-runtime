#ifndef _RIVE_KEYED_PROPERTY_BASE_HPP_
#define _RIVE_KEYED_PROPERTY_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class KeyedPropertyBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 26;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case KeyedPropertyBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t propertyKeyPropertyKey = 53;

protected:
    uint32_t m_PropertyKey = Core::invalidPropertyKey;

public:
    inline uint32_t propertyKey() const { return m_PropertyKey; }
    void propertyKey(uint32_t value)
    {
        if (m_PropertyKey == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(propertyKeyPropertyKey, &m_PropertyKey, &value);
        m_PropertyKey = value;
        RIVE_EDITOR_CHANGED(propertyKeyChanged());
        notifyPropertyChanged(propertyKeyPropertyKey);
    }

    Core* clone() const override;
    void copy(const KeyedPropertyBase& object)
    {
        m_PropertyKey = object.m_PropertyKey;
        RIVE_EDITOR_COPY(object);
        RIVE_EDITOR_COPY_VALIDATED(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case propertyKeyPropertyKey:
                m_PropertyKey = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return false;
    }

protected:
    virtual void propertyKeyChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/keyed_property_ext.inl"
#endif
};
} // namespace rive

#endif