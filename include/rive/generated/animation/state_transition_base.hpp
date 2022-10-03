#ifndef _RIVE_STATE_TRANSITION_BASE_HPP_
#define _RIVE_STATE_TRANSITION_BASE_HPP_
#include "rive/animation/state_machine_layer_component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class StateTransitionBase : public StateMachineLayerComponent
{
protected:
    typedef StateMachineLayerComponent Super;

public:
    static const uint16_t typeKey = 65;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StateTransitionBase::typeKey:
            case StateMachineLayerComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t stateToIdPropertyKey = 151;
    static const uint16_t flagsPropertyKey = 152;
    static const uint16_t durationPropertyKey = 158;
    static const uint16_t exitTimePropertyKey = 160;

private:
    uint32_t m_StateToId = -1;
    uint32_t m_Flags = 0;
    uint32_t m_Duration = 0;
    uint32_t m_ExitTime = 0;

public:
    inline uint32_t stateToId() const { return m_StateToId; }
    void stateToId(uint32_t value)
    {
        if (m_StateToId == value)
        {
            return;
        }
        m_StateToId = value;
        stateToIdChanged();
    }

    inline uint32_t flags() const { return m_Flags; }
    void flags(uint32_t value)
    {
        if (m_Flags == value)
        {
            return;
        }
        m_Flags = value;
        flagsChanged();
    }

    inline uint32_t duration() const { return m_Duration; }
    void duration(uint32_t value)
    {
        if (m_Duration == value)
        {
            return;
        }
        m_Duration = value;
        durationChanged();
    }

    inline uint32_t exitTime() const { return m_ExitTime; }
    void exitTime(uint32_t value)
    {
        if (m_ExitTime == value)
        {
            return;
        }
        m_ExitTime = value;
        exitTimeChanged();
    }

    Core* clone() const override;
    void copy(const StateTransitionBase& object)
    {
        m_StateToId = object.m_StateToId;
        m_Flags = object.m_Flags;
        m_Duration = object.m_Duration;
        m_ExitTime = object.m_ExitTime;
        StateMachineLayerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case stateToIdPropertyKey:
                m_StateToId = CoreUintType::deserialize(reader);
                return true;
            case flagsPropertyKey:
                m_Flags = CoreUintType::deserialize(reader);
                return true;
            case durationPropertyKey:
                m_Duration = CoreUintType::deserialize(reader);
                return true;
            case exitTimePropertyKey:
                m_ExitTime = CoreUintType::deserialize(reader);
                return true;
        }
        return StateMachineLayerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void stateToIdChanged() {}
    virtual void flagsChanged() {}
    virtual void durationChanged() {}
    virtual void exitTimeChanged() {}
};
} // namespace rive

#endif