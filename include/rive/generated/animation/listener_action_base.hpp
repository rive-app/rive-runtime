#ifndef _RIVE_LISTENER_ACTION_BASE_HPP_
#define _RIVE_LISTENER_ACTION_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ListenerActionBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 125;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t flagsPropertyKey = 980;

protected:
    uint32_t m_Flags = 0;

public:
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

    void copy(const ListenerActionBase& object)
    {
        m_Flags = object.m_Flags;
        RIVE_EDITOR_COPY(object);
        RIVE_EDITOR_COPY_VALIDATED(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case flagsPropertyKey:
                m_Flags = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return false;
    }

protected:
    virtual void flagsChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/listener_action_ext.inl"
#endif
};
} // namespace rive

#endif