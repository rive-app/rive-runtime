#ifndef _RIVE_STATE_MACHINE_LISTENER_SINGLE_BASE_HPP_
#define _RIVE_STATE_MACHINE_LISTENER_SINGLE_BASE_HPP_
#include "rive/animation/state_machine_listener.hpp"
#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
#include "rive/span.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class StateMachineListenerSingleBase : public StateMachineListener
{
protected:
    typedef StateMachineListener Super;

public:
    static const uint16_t typeKey = 114;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StateMachineListenerSingleBase::typeKey:
            case StateMachineListenerBase::typeKey:
            case StateMachineComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t listenerTypeValuePropertyKey = 225;
    static const uint16_t eventIdPropertyKey = 399;
    static const uint16_t viewModelPathIdsPropertyKey = 868;

protected:
    uint32_t m_ListenerTypeValue = 0;
    Id m_EventId = kEmptyId;

public:
    inline uint32_t listenerTypeValue() const { return m_ListenerTypeValue; }
    void listenerTypeValue(uint32_t value)
    {
        if (m_ListenerTypeValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(listenerTypeValuePropertyKey,
                             &m_ListenerTypeValue,
                             &value);
        m_ListenerTypeValue = value;
        RIVE_EDITOR_CHANGED(listenerTypeValueChanged());
        notifyPropertyChanged(listenerTypeValuePropertyKey);
    }

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

    virtual void decodeViewModelPathIds(Span<const uint8_t> value) = 0;
    virtual void copyViewModelPathIds(
        const StateMachineListenerSingleBase& object) = 0;

    Core* clone() const override;
    void copy(const StateMachineListenerSingleBase& object)
    {
        m_ListenerTypeValue = object.m_ListenerTypeValue;
        m_EventId = object.m_EventId;
        copyViewModelPathIds(object);
        RIVE_EDITOR_COPY(object);
        StateMachineListener::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case listenerTypeValuePropertyKey:
                m_ListenerTypeValue = CoreUintType::deserialize(reader);
                return true;
            case eventIdPropertyKey:
                m_EventId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case viewModelPathIdsPropertyKey:
                decodeViewModelPathIds(CoreBytesType::deserialize(reader));
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return StateMachineListener::deserialize(propertyKey, reader);
    }

protected:
    virtual void listenerTypeValueChanged() {}
    virtual void eventIdChanged() {}
    virtual void viewModelPathIdsChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/state_machine_listener_single_ext.inl"
#endif
};
} // namespace rive

#endif