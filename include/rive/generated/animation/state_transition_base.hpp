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
    static const uint16_t interpolationTypePropertyKey = 349;
    static const uint16_t interpolatorIdPropertyKey = 350;
    static const uint16_t randomWeightPropertyKey = 537;

private:
    uint32_t m_StateToId = -1;
    uint32_t m_Flags = 0;
    uint32_t m_Duration = 0;
    uint32_t m_ExitTime = 0;
    uint32_t m_InterpolationType = 1;
    uint32_t m_InterpolatorId = -1;
    uint32_t m_RandomWeight = 1;

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

    inline uint32_t interpolationType() const { return m_InterpolationType; }
    void interpolationType(uint32_t value)
    {
        if (m_InterpolationType == value)
        {
            return;
        }
        m_InterpolationType = value;
        interpolationTypeChanged();
    }

    inline uint32_t interpolatorId() const { return m_InterpolatorId; }
    void interpolatorId(uint32_t value)
    {
        if (m_InterpolatorId == value)
        {
            return;
        }
        m_InterpolatorId = value;
        interpolatorIdChanged();
    }

    inline uint32_t randomWeight() const { return m_RandomWeight; }
    void randomWeight(uint32_t value)
    {
        if (m_RandomWeight == value)
        {
            return;
        }
        m_RandomWeight = value;
        randomWeightChanged();
    }

    Core* clone() const override;
    void copy(const StateTransitionBase& object)
    {
        m_StateToId = object.m_StateToId;
        m_Flags = object.m_Flags;
        m_Duration = object.m_Duration;
        m_ExitTime = object.m_ExitTime;
        m_InterpolationType = object.m_InterpolationType;
        m_InterpolatorId = object.m_InterpolatorId;
        m_RandomWeight = object.m_RandomWeight;
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
            case interpolationTypePropertyKey:
                m_InterpolationType = CoreUintType::deserialize(reader);
                return true;
            case interpolatorIdPropertyKey:
                m_InterpolatorId = CoreUintType::deserialize(reader);
                return true;
            case randomWeightPropertyKey:
                m_RandomWeight = CoreUintType::deserialize(reader);
                return true;
        }
        return StateMachineLayerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void stateToIdChanged() {}
    virtual void flagsChanged() {}
    virtual void durationChanged() {}
    virtual void exitTimeChanged() {}
    virtual void interpolationTypeChanged() {}
    virtual void interpolatorIdChanged() {}
    virtual void randomWeightChanged() {}
};
} // namespace rive

#endif