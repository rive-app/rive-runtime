#ifndef _RIVE_DATA_BIND_BASE_HPP_
#define _RIVE_DATA_BIND_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class DataBindBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 446;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataBindBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t propertyKeyPropertyKey = 586;
    static const uint16_t flagsPropertyKey = 587;
    static const uint16_t converterIdPropertyKey = 660;

protected:
    uint32_t m_PropertyKey = Core::invalidPropertyKey;
    uint32_t m_Flags = 0;
    Id m_ConverterId = kEmptyId;

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

    inline uint32_t flags() const { return m_Flags; }
    void flags(uint32_t value)
    {
        if (m_Flags == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(flagsPropertyKey, &m_Flags, &value);
        m_Flags = value;
        RIVE_EDITOR_CHANGED(flagsChanged());
        notifyPropertyChanged(flagsPropertyKey);
    }

    inline Id converterId() const { return m_ConverterId; }
    void converterId(Id value)
    {
        if (m_ConverterId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(converterIdPropertyKey, &m_ConverterId, &value);
        m_ConverterId = value;
        RIVE_EDITOR_CHANGED(converterIdChanged());
        notifyPropertyChanged(converterIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const DataBindBase& object)
    {
        m_PropertyKey = object.m_PropertyKey;
        m_Flags = object.m_Flags;
        m_ConverterId = object.m_ConverterId;
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
            case flagsPropertyKey:
                m_Flags = CoreUintType::deserialize(reader);
                return true;
            case converterIdPropertyKey:
                m_ConverterId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return false;
    }

protected:
    virtual void propertyKeyChanged() {}
    virtual void flagsChanged() {}
    virtual void converterIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/data_bind/data_bind_ext.inl"
#endif
};
} // namespace rive

#endif