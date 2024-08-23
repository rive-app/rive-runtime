#ifndef _RIVE_TRANSITION_PROPERTY_VIEW_MODEL_COMPARATOR_BASE_HPP_
#define _RIVE_TRANSITION_PROPERTY_VIEW_MODEL_COMPARATOR_BASE_HPP_
#include "rive/animation/transition_property_comparator.hpp"
namespace rive
{
class TransitionPropertyViewModelComparatorBase : public TransitionPropertyComparator
{
protected:
    typedef TransitionPropertyComparator Super;

public:
    static const uint16_t typeKey = 479;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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

    Core* clone() const override;

protected:
};
} // namespace rive

#endif