#ifndef _RIVE_TRANSITION_PROPERTY_ARTBOARD_COMPARATOR_BASE_HPP_
#define _RIVE_TRANSITION_PROPERTY_ARTBOARD_COMPARATOR_BASE_HPP_
#include "rive/animation/transition_property_comparator.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TransitionPropertyArtboardComparatorBase
    : public TransitionPropertyComparator
{
protected:
    typedef TransitionPropertyComparator Super;

public:
    static const uint16_t typeKey = 496;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionPropertyArtboardComparatorBase::typeKey:
            case TransitionPropertyComparatorBase::typeKey:
            case TransitionComparatorBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t propertyTypePropertyKey = 677;

protected:
    uint32_t m_PropertyType = 0;

public:
    inline uint32_t propertyType() const { return m_PropertyType; }
    void propertyType(uint32_t value)
    {
        if (m_PropertyType == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(propertyTypePropertyKey, &m_PropertyType, &value);
        m_PropertyType = value;
        RIVE_EDITOR_CHANGED(propertyTypeChanged());
        notifyPropertyChanged(propertyTypePropertyKey);
    }

    Core* clone() const override;
    void copy(const TransitionPropertyArtboardComparatorBase& object)
    {
        m_PropertyType = object.m_PropertyType;
        TransitionPropertyComparator::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case propertyTypePropertyKey:
                m_PropertyType = CoreUintType::deserialize(reader);
                return true;
        }
        return TransitionPropertyComparator::deserialize(propertyKey, reader);
    }

protected:
    virtual void propertyTypeChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/transition_property_artboard_comparator_ext.inl"
#endif
};
} // namespace rive

#endif