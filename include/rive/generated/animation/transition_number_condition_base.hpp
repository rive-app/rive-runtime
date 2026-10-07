#ifndef _RIVE_TRANSITION_NUMBER_CONDITION_BASE_HPP_
#define _RIVE_TRANSITION_NUMBER_CONDITION_BASE_HPP_
#include "rive/animation/transition_value_condition.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class TransitionNumberConditionBase : public TransitionValueCondition
{
protected:
    typedef TransitionValueCondition Super;

public:
    static const uint16_t typeKey = 70;

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t valuePropertyKey = 157;

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
    void copy(const TransitionNumberConditionBase& object)
    {
        m_Value = object.m_Value;
        TransitionValueCondition::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case valuePropertyKey:
                m_Value = CoreDoubleType::deserialize(reader);
                return true;
        }
        return TransitionValueCondition::deserialize(propertyKey, reader);
    }

protected:
    virtual void valueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/transition_number_condition_ext.inl"
#endif
};
} // namespace rive

#endif