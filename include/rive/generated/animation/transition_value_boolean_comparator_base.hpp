#ifndef _RIVE_TRANSITION_VALUE_BOOLEAN_COMPARATOR_BASE_HPP_
#define _RIVE_TRANSITION_VALUE_BOOLEAN_COMPARATOR_BASE_HPP_
#include "rive/animation/transition_value_comparator.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
namespace rive
{
class TransitionValueBooleanComparatorBase : public TransitionValueComparator
{
protected:
    typedef TransitionValueComparator Super;

public:
    static const uint16_t typeKey = 481;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionValueBooleanComparatorBase::typeKey:
            case TransitionValueComparatorBase::typeKey:
            case TransitionComparatorBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t valuePropertyKey = 647;

protected:
    bool m_Value = false;

public:
    inline bool value() const { return m_Value; }
    void value(bool value)
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
    void copy(const TransitionValueBooleanComparatorBase& object)
    {
        m_Value = object.m_Value;
        TransitionValueComparator::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case valuePropertyKey:
                m_Value = CoreBoolType::deserialize(reader);
                return true;
        }
        return TransitionValueComparator::deserialize(propertyKey, reader);
    }

protected:
    virtual void valueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/transition_value_boolean_comparator_ext.inl"
#endif
};
} // namespace rive

#endif