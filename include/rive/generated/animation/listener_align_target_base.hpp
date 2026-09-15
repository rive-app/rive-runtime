#ifndef _RIVE_LISTENER_ALIGN_TARGET_BASE_HPP_
#define _RIVE_LISTENER_ALIGN_TARGET_BASE_HPP_
#include "rive/animation/listener_action.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class ListenerAlignTargetBase : public ListenerAction
{
protected:
    typedef ListenerAction Super;

public:
    static const uint16_t typeKey = 126;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerAlignTargetBase::typeKey:
            case ListenerActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t targetIdPropertyKey = 240;
    static const uint16_t preserveOffsetPropertyKey = 541;

protected:
    Id m_TargetId = kEmptyId;
    bool m_PreserveOffset = false;

public:
    inline Id targetId() const { return m_TargetId; }
    void targetId(Id value)
    {
        if (m_TargetId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(targetIdPropertyKey, &m_TargetId, &value);
        m_TargetId = value;
        RIVE_EDITOR_CHANGED(targetIdChanged());
        notifyPropertyChanged(targetIdPropertyKey);
    }

    inline bool preserveOffset() const { return m_PreserveOffset; }
    void preserveOffset(bool value)
    {
        if (m_PreserveOffset == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(preserveOffsetPropertyKey,
                             &m_PreserveOffset,
                             &value);
        m_PreserveOffset = value;
        RIVE_EDITOR_CHANGED(preserveOffsetChanged());
        notifyPropertyChanged(preserveOffsetPropertyKey);
    }

    Core* clone() const override;
    void copy(const ListenerAlignTargetBase& object)
    {
        m_TargetId = object.m_TargetId;
        m_PreserveOffset = object.m_PreserveOffset;
        ListenerAction::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case targetIdPropertyKey:
                m_TargetId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case preserveOffsetPropertyKey:
                m_PreserveOffset = CoreBoolType::deserialize(reader);
                return true;
        }
        return ListenerAction::deserialize(propertyKey, reader);
    }

protected:
    virtual void targetIdChanged() {}
    virtual void preserveOffsetChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/listener_align_target_ext.inl"
#endif
};
} // namespace rive

#endif