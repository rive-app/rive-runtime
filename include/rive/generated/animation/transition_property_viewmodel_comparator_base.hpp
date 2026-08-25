#ifndef _RIVE_TRANSITION_PROPERTY_VIEW_MODEL_COMPARATOR_BASE_HPP_
#define _RIVE_TRANSITION_PROPERTY_VIEW_MODEL_COMPARATOR_BASE_HPP_
#include "rive/animation/transition_property_comparator.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class TransitionPropertyViewModelComparatorBase
    : public TransitionPropertyComparator
{
protected:
    typedef TransitionPropertyComparator Super;

public:
    static const uint16_t typeKey = 479;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionPropertyViewModelComparatorBase::typeKey:
            case TransitionPropertyComparatorBase::typeKey:
            case TransitionComparatorBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

public:
    Core* clone() const override;
    void copy(const TransitionPropertyViewModelComparatorBase& object)
    {
        RIVE_EDITOR_COPY(object);
        TransitionPropertyComparator::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return TransitionPropertyComparator::deserialize(propertyKey, reader);
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/transition_property_viewmodel_comparator_ext.inl"
#endif
};
} // namespace rive

#endif