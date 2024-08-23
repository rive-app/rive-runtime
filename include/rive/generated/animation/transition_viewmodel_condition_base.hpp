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

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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

    static const uint16_t leftComparatorIdPropertyKey = 648;
    static const uint16_t rightComparatorIdPropertyKey = 649;
    static const uint16_t opValuePropertyKey = 650;

private:
    uint32_t m_LeftComparatorId = -1;
    uint32_t m_RightComparatorId = -1;
    uint32_t m_OpValue = 0;

public:
    inline uint32_t leftComparatorId() const { return m_LeftComparatorId; }
    void leftComparatorId(uint32_t value)
    {
        if (m_LeftComparatorId == value)
        {
            return;
        }
        m_LeftComparatorId = value;
        leftComparatorIdChanged();
    }

    inline uint32_t rightComparatorId() const { return m_RightComparatorId; }
    void rightComparatorId(uint32_t value)
    {
        if (m_RightComparatorId == value)
        {
            return;
        }
        m_RightComparatorId = value;
        rightComparatorIdChanged();
    }

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
        m_LeftComparatorId = object.m_LeftComparatorId;
        m_RightComparatorId = object.m_RightComparatorId;
        m_OpValue = object.m_OpValue;
        TransitionCondition::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case leftComparatorIdPropertyKey:
                m_LeftComparatorId = CoreUintType::deserialize(reader);
                return true;
            case rightComparatorIdPropertyKey:
                m_RightComparatorId = CoreUintType::deserialize(reader);
                return true;
            case opValuePropertyKey:
                m_OpValue = CoreUintType::deserialize(reader);
                return true;
        }
        return TransitionCondition::deserialize(propertyKey, reader);
    }

protected:
    virtual void leftComparatorIdChanged() {}
    virtual void rightComparatorIdChanged() {}
    virtual void opValueChanged() {}
};
} // namespace rive

#endif