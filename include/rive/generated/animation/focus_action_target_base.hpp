#ifndef _RIVE_FOCUS_ACTION_TARGET_BASE_HPP_
#define _RIVE_FOCUS_ACTION_TARGET_BASE_HPP_
#include "rive/animation/focus_action.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class FocusActionTargetBase : public FocusAction
{
protected:
    typedef FocusAction Super;

public:
    static const uint16_t typeKey = 652;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FocusActionTargetBase::typeKey:
            case FocusActionBase::typeKey:
            case ListenerActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t targetIdPropertyKey = 952;

protected:
    Id m_TargetId = kEmptyId;

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

    Core* clone() const override;
    void copy(const FocusActionTargetBase& object)
    {
        m_TargetId = object.m_TargetId;
        FocusAction::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case targetIdPropertyKey:
                m_TargetId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return FocusAction::deserialize(propertyKey, reader);
    }

protected:
    virtual void targetIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/focus_action_target_ext.inl"
#endif
};
} // namespace rive

#endif