#ifndef _RIVE_STATE_TRANSITION_BASE_HPP_
#define _RIVE_STATE_TRANSITION_BASE_HPP_
#include "rive/animation/state_machine_layer_component.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class StateTransitionBase : public StateMachineLayerComponent
{
protected:
    typedef StateMachineLayerComponent Super;

public:
    static const uint16_t typeKey = 65;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
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

protected:
    Id m_StateToId = kEmptyId;
    uint32_t m_Flags = 0;
    uint32_t m_Duration = 0;
    uint32_t m_ExitTime = 0;
    uint32_t m_InterpolationType = 1;
    Id m_InterpolatorId = kEmptyId;
    uint32_t m_RandomWeight = 1;

public:
    inline Id stateToId() const { return m_StateToId; }
    void stateToId(Id value)
    {
        if (m_StateToId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(stateToIdPropertyKey, &m_StateToId, &value);
        m_StateToId = value;
        RIVE_EDITOR_CHANGED(stateToIdChanged());
        notifyPropertyChanged(stateToIdPropertyKey);
    }

    inline uint32_t flags() const { return m_Flags; }
    void flags(uint32_t value)
    {
        if (m_Flags == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(flagsPropertyKey, &m_Flags, &value);
        m_Flags = value;
        RIVE_EDITOR_CHANGED(flagsChanged());
        notifyPropertyChanged(flagsPropertyKey);
    }

    inline uint32_t duration() const { return m_Duration; }
    void duration(uint32_t value)
    {
        if (m_Duration == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(durationPropertyKey, &m_Duration, &value);
        m_Duration = value;
        RIVE_EDITOR_CHANGED(durationChanged());
        notifyPropertyChanged(durationPropertyKey);
    }

    inline uint32_t exitTime() const { return m_ExitTime; }
    void exitTime(uint32_t value)
    {
        if (m_ExitTime == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(exitTimePropertyKey, &m_ExitTime, &value);
        m_ExitTime = value;
        RIVE_EDITOR_CHANGED(exitTimeChanged());
        notifyPropertyChanged(exitTimePropertyKey);
    }

    inline uint32_t interpolationType() const { return m_InterpolationType; }
    void interpolationType(uint32_t value)
    {
        if (m_InterpolationType == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(interpolationTypePropertyKey,
                             &m_InterpolationType,
                             &value);
        m_InterpolationType = value;
        RIVE_EDITOR_CHANGED(interpolationTypeChanged());
        notifyPropertyChanged(interpolationTypePropertyKey);
    }

    inline Id interpolatorId() const { return m_InterpolatorId; }
    void interpolatorId(Id value)
    {
        if (m_InterpolatorId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(interpolatorIdPropertyKey,
                             &m_InterpolatorId,
                             &value);
        m_InterpolatorId = value;
        RIVE_EDITOR_CHANGED(interpolatorIdChanged());
        notifyPropertyChanged(interpolatorIdPropertyKey);
    }

    inline uint32_t randomWeight() const { return m_RandomWeight; }
    void randomWeight(uint32_t value)
    {
        if (m_RandomWeight == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(randomWeightPropertyKey, &m_RandomWeight, &value);
        m_RandomWeight = value;
        RIVE_EDITOR_CHANGED(randomWeightChanged());
        notifyPropertyChanged(randomWeightPropertyKey);
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
        RIVE_EDITOR_COPY(object);
        StateMachineLayerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case stateToIdPropertyKey:
                m_StateToId = CoreIdType::runtimeDeserialize(reader);
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
                m_InterpolatorId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case randomWeightPropertyKey:
                m_RandomWeight = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
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
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/state_transition_ext.inl"
#endif
};
} // namespace rive

#endif