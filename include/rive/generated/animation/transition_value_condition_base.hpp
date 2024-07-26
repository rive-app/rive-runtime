#ifndef _RIVE_TRANSITION_VALUE_CONDITION_BASE_HPP_
#define _RIVE_TRANSITION_VALUE_CONDITION_BASE_HPP_
#include "rive/animation/transition_input_condition.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TransitionValueConditionBase : public TransitionInputCondition
{
protected:
    typedef TransitionInputCondition Super;

public:
    static const uint16_t typeKey = 69;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionValueConditionBase::typeKey:
            case TransitionInputConditionBase::typeKey:
            case TransitionConditionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t opValuePropertyKey = 156;

private:
    uint32_t m_OpValue = 0;

public:
    inline uint32_t opValue() const { return m_OpValue; }
    void opValue(uint32_t value)
    {
        if (m_OpValue == value)
        {
            return;
        }
        m_OpValue = value;
        opValueChanged();
    }

    void copy(const TransitionValueConditionBase& object)
    {
        m_OpValue = object.m_OpValue;
        TransitionInputCondition::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case opValuePropertyKey:
                m_OpValue = CoreUintType::deserialize(reader);
                return true;
        }
        return TransitionInputCondition::deserialize(propertyKey, reader);
    }

protected:
    virtual void opValueChanged() {}
};
} // namespace rive

#endif