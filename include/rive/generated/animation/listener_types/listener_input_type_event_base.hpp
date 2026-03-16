#ifndef _RIVE_LISTENER_INPUT_TYPE_EVENT_BASE_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_EVENT_BASE_HPP_
#include "rive/animation/listener_types/listener_input_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ListenerInputTypeEventBase : public ListenerInputType
{
protected:
    typedef ListenerInputType Super;

public:
    static const uint16_t typeKey = 659;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerInputTypeEventBase::typeKey:
            case ListenerInputTypeBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t eventIdPropertyKey = 962;

protected:
    uint32_t m_EventId = -1;

public:
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
    void copy(const ListenerInputTypeEventBase& object)
    {
        m_EventId = object.m_EventId;
        ListenerInputType::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case eventIdPropertyKey:
                m_EventId = CoreUintType::deserialize(reader);
                return true;
        }
        return ListenerInputType::deserialize(propertyKey, reader);
    }

protected:
    virtual void eventIdChanged() {}
};
} // namespace rive

#endif