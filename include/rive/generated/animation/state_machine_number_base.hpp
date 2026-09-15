#ifndef _RIVE_STATE_MACHINE_NUMBER_BASE_HPP_
#define _RIVE_STATE_MACHINE_NUMBER_BASE_HPP_
#include "rive/animation/state_machine_input.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class StateMachineNumberBase : public StateMachineInput
{
protected:
    typedef StateMachineInput Super;

public:
    static const uint16_t typeKey = 56;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StateMachineNumberBase::typeKey:
            case StateMachineInputBase::typeKey:
            case StateMachineComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t valuePropertyKey = 140;

protected:
    float m_Value = 0.0f;

public:
    inline float value() const { return m_Value; }
    void value(float value)
    {
        if (m_Value == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(valuePropertyKey, &m_Value, &value);
        m_Value = value;
        RIVE_EDITOR_CHANGED(valueChanged());
        notifyPropertyChanged(valuePropertyKey);
    }

    Core* clone() const override;
    void copy(const StateMachineNumberBase& object)
    {
        m_Value = object.m_Value;
        RIVE_EDITOR_COPY(object);
        StateMachineInput::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case valuePropertyKey:
                m_Value = CoreDoubleType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return StateMachineInput::deserialize(propertyKey, reader);
    }

protected:
    virtual void valueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/state_machine_number_ext.inl"
#endif
};
} // namespace rive

#endif