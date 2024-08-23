#ifndef _RIVE_STATE_MACHINE_FIRE_EVENT_BASE_HPP_
#define _RIVE_STATE_MACHINE_FIRE_EVENT_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class StateMachineFireEventBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 169;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StateMachineFireEventBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t eventIdPropertyKey = 392;
    static const uint16_t occursValuePropertyKey = 393;

private:
    uint32_t m_EventId = -1;
    uint32_t m_OccursValue = 0;

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

    inline uint32_t occursValue() const { return m_OccursValue; }
    void occursValue(uint32_t value)
    {
        if (m_OccursValue == value)
        {
            return;
        }
        m_OccursValue = value;
        occursValueChanged();
    }

    Core* clone() const override;
    void copy(const StateMachineFireEventBase& object)
    {
        m_EventId = object.m_EventId;
        m_OccursValue = object.m_OccursValue;
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case eventIdPropertyKey:
                m_EventId = CoreUintType::deserialize(reader);
                return true;
            case occursValuePropertyKey:
                m_OccursValue = CoreUintType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void eventIdChanged() {}
    virtual void occursValueChanged() {}
};
} // namespace rive

#endif