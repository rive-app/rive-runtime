#ifndef _RIVE_STATE_MACHINE_LISTENER_BASE_HPP_
#define _RIVE_STATE_MACHINE_LISTENER_BASE_HPP_
#include "rive/animation/state_machine_component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class StateMachineListenerBase : public StateMachineComponent
{
protected:
    typedef StateMachineComponent Super;

public:
    static const uint16_t typeKey = 114;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StateMachineListenerBase::typeKey:
            case StateMachineComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t targetIdPropertyKey = 224;
    static const uint16_t listenerTypeValuePropertyKey = 225;
    static const uint16_t eventIdPropertyKey = 399;

private:
    uint32_t m_TargetId = 0;
    uint32_t m_ListenerTypeValue = 0;
    uint32_t m_EventId = -1;

public:
    inline uint32_t targetId() const { return m_TargetId; }
    void targetId(uint32_t value)
    {
        if (m_TargetId == value)
        {
            return;
        }
        m_TargetId = value;
        targetIdChanged();
    }

    inline uint32_t listenerTypeValue() const { return m_ListenerTypeValue; }
    void listenerTypeValue(uint32_t value)
    {
        if (m_ListenerTypeValue == value)
        {
            return;
        }
        m_ListenerTypeValue = value;
        listenerTypeValueChanged();
    }

    inline uint32_t eventId() const { return m_EventId; }
    void eventId(uint32_t value)
    {
        if (m_EventId == value)
        {
            return;
        }
        m_EventId = value;
        eventIdChanged();
    }

    Core* clone() const override;
    void copy(const StateMachineListenerBase& object)
    {
        m_TargetId = object.m_TargetId;
        m_ListenerTypeValue = object.m_ListenerTypeValue;
        m_EventId = object.m_EventId;
        StateMachineComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case targetIdPropertyKey:
                m_TargetId = CoreUintType::deserialize(reader);
                return true;
            case listenerTypeValuePropertyKey:
                m_ListenerTypeValue = CoreUintType::deserialize(reader);
                return true;
            case eventIdPropertyKey:
                m_EventId = CoreUintType::deserialize(reader);
                return true;
        }
        return StateMachineComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void targetIdChanged() {}
    virtual void listenerTypeValueChanged() {}
    virtual void eventIdChanged() {}
};
} // namespace rive

#endif