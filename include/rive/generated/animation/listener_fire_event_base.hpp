#ifndef _RIVE_LISTENER_FIRE_EVENT_BASE_HPP_
#define _RIVE_LISTENER_FIRE_EVENT_BASE_HPP_
#include "rive/animation/listener_action.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class ListenerFireEventBase : public ListenerAction
{
protected:
    typedef ListenerAction Super;

public:
    static const uint16_t typeKey = 168;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerFireEventBase::typeKey:
            case ListenerActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t eventIdPropertyKey = 389;

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
    void copy(const ListenerFireEventBase& object)
    {
        m_EventId = object.m_EventId;
        ListenerAction::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case eventIdPropertyKey:
                m_EventId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return ListenerAction::deserialize(propertyKey, reader);
    }

protected:
    virtual void eventIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/listener_fire_event_ext.inl"
#endif
};
} // namespace rive

#endif