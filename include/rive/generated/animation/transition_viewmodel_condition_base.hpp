#ifndef _RIVE_TRANSITION_VIEW_MODEL_CONDITION_BASE_HPP_
#define _RIVE_TRANSITION_VIEW_MODEL_CONDITION_BASE_HPP_
#include "rive/animation/transition_condition.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TransitionViewModelConditionBase : public TransitionCondition
{
protected:
    typedef TransitionCondition Super;

public:
    static const uint16_t typeKey = 482;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionViewModelConditionBase::typeKey:
            case TransitionConditionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t opValuePropertyKey = 650;

protected:
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

    Core* clone() const override;
    void copy(const TransitionViewModelConditionBase& object)
    {
        m_OpValue = object.m_OpValue;
        TransitionCondition::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case opValuePropertyKey:
                m_OpValue = CoreUintType::deserialize(reader);
                return true;
        }
        return TransitionCondition::deserialize(propertyKey, reader);
    }

protected:
    virtual void opValueChanged() {}
};
} // namespace rive

#endif