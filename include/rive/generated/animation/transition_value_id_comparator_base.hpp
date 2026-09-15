#ifndef _RIVE_TRANSITION_VALUE_ID_COMPARATOR_BASE_HPP_
#define _RIVE_TRANSITION_VALUE_ID_COMPARATOR_BASE_HPP_
#include "rive/animation/transition_value_comparator.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class TransitionValueIdComparatorBase : public TransitionValueComparator
{
protected:
    typedef TransitionValueComparator Super;

public:
    static const uint16_t typeKey = 601;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionValueIdComparatorBase::typeKey:
            case TransitionValueComparatorBase::typeKey:
            case TransitionComparatorBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t valuePropertyKey = 653;

protected:
    Id m_Value = kEmptyId;

public:
    inline Id value() const { return m_Value; }
    void value(Id value)
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
    void copy(const TransitionValueIdComparatorBase& object)
    {
        m_Value = object.m_Value;
        TransitionValueComparator::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case valuePropertyKey:
                m_Value = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return TransitionValueComparator::deserialize(propertyKey, reader);
    }

protected:
    virtual void valueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/transition_value_id_comparator_ext.inl"
#endif
};
} // namespace rive

#endif