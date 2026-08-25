#ifndef _RIVE_STATE_MACHINE_FIRE_ACTION_BASE_HPP_
#define _RIVE_STATE_MACHINE_FIRE_ACTION_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class StateMachineFireActionBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 615;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StateMachineFireActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t occursValuePropertyKey = 393;

protected:
    uint8_t m_OccursValue = 0;

public:
    inline uint8_t occursValue() const { return m_OccursValue; }
    void occursValue(uint8_t value)
    {
        if (m_OccursValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(occursValuePropertyKey, &m_OccursValue, &value);
        m_OccursValue = value;
        RIVE_EDITOR_CHANGED(occursValueChanged());
        notifyPropertyChanged(occursValuePropertyKey);
    }

    void copy(const StateMachineFireActionBase& object)
    {
        m_OccursValue = object.m_OccursValue;
        RIVE_EDITOR_COPY(object);
        RIVE_EDITOR_COPY_VALIDATED(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case occursValuePropertyKey:
                m_OccursValue = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return false;
    }

protected:
    virtual void occursValueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/state_machine_fire_action_ext.inl"
#endif
};
} // namespace rive

#endif