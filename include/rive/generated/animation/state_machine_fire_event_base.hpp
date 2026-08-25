#ifndef _RIVE_STATE_MACHINE_FIRE_EVENT_BASE_HPP_
#define _RIVE_STATE_MACHINE_FIRE_EVENT_BASE_HPP_
#include "rive/animation/state_machine_fire_action.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class StateMachineFireEventBase : public StateMachineFireAction
{
protected:
    typedef StateMachineFireAction Super;

public:
    static const uint16_t typeKey = 169;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StateMachineFireEventBase::typeKey:
            case StateMachineFireActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t eventIdPropertyKey = 392;

protected:
    Id m_EventId = kEmptyId;

public:
    inline Id eventId() const { return m_EventId; }
    void eventId(Id value)
    {
        if (m_EventId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(eventIdPropertyKey, &m_EventId, &value);
        m_EventId = value;
        RIVE_EDITOR_CHANGED(eventIdChanged());
        notifyPropertyChanged(eventIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const StateMachineFireEventBase& object)
    {
        m_EventId = object.m_EventId;
        StateMachineFireAction::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case eventIdPropertyKey:
                m_EventId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return StateMachineFireAction::deserialize(propertyKey, reader);
    }

protected:
    virtual void eventIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/state_machine_fire_event_ext.inl"
#endif
};
} // namespace rive

#endif