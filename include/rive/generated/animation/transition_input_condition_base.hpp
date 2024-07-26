#ifndef _RIVE_TRANSITION_INPUT_CONDITION_BASE_HPP_
#define _RIVE_TRANSITION_INPUT_CONDITION_BASE_HPP_
#include "rive/animation/transition_condition.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TransitionInputConditionBase : public TransitionCondition
{
protected:
    typedef TransitionCondition Super;

public:
    static const uint16_t typeKey = 67;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionInputConditionBase::typeKey:
            case TransitionConditionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t inputIdPropertyKey = 155;

private:
    uint32_t m_InputId = -1;

public:
    inline uint32_t inputId() const { return m_InputId; }
    void inputId(uint32_t value)
    {
        if (m_InputId == value)
        {
            return;
        }
        m_InputId = value;
        inputIdChanged();
    }

    void copy(const TransitionInputConditionBase& object)
    {
        m_InputId = object.m_InputId;
        TransitionCondition::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case inputIdPropertyKey:
                m_InputId = CoreUintType::deserialize(reader);
                return true;
        }
        return TransitionCondition::deserialize(propertyKey, reader);
    }

protected:
    virtual void inputIdChanged() {}
};
} // namespace rive

#endif