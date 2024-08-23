#ifndef _RIVE_TRANSITION_VALUE_ENUM_COMPARATOR_BASE_HPP_
#define _RIVE_TRANSITION_VALUE_ENUM_COMPARATOR_BASE_HPP_
#include "rive/animation/transition_value_comparator.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TransitionValueEnumComparatorBase : public TransitionValueComparator
{
protected:
    typedef TransitionValueComparator Super;

public:
    static const uint16_t typeKey = 485;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionValueEnumComparatorBase::typeKey:
            case TransitionValueComparatorBase::typeKey:
            case TransitionComparatorBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t valuePropertyKey = 653;

private:
    uint32_t m_Value = -1;

public:
    inline uint32_t value() const { return m_Value; }
    void value(uint32_t value)
    {
        if (m_Value == value)
        {
            return;
        }
        m_Value = value;
        valueChanged();
    }

    Core* clone() const override;
    void copy(const TransitionValueEnumComparatorBase& object)
    {
        m_Value = object.m_Value;
        TransitionValueComparator::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case valuePropertyKey:
                m_Value = CoreUintType::deserialize(reader);
                return true;
        }
        return TransitionValueComparator::deserialize(propertyKey, reader);
    }

protected:
    virtual void valueChanged() {}
};
} // namespace rive

#endif