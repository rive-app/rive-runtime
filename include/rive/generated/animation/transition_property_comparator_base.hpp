#ifndef _RIVE_TRANSITION_PROPERTY_COMPARATOR_BASE_HPP_
#define _RIVE_TRANSITION_PROPERTY_COMPARATOR_BASE_HPP_
#include "rive/animation/transition_comparator.hpp"
namespace rive
{
class TransitionPropertyComparatorBase : public TransitionComparator
{
protected:
    typedef TransitionComparator Super;

public:
    static const uint16_t typeKey = 478;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionPropertyComparatorBase::typeKey:
            case TransitionComparatorBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

protected:
};
} // namespace rive

#endif