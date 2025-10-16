#ifndef _RIVE_TRANSITION_VALUE_ARTBOARD_COMPARATOR_BASE_HPP_
#define _RIVE_TRANSITION_VALUE_ARTBOARD_COMPARATOR_BASE_HPP_
#include "rive/animation/transition_value_id_comparator.hpp"
namespace rive
{
class TransitionValueArtboardComparatorBase : public TransitionValueIdComparator
{
protected:
    typedef TransitionValueIdComparator Super;

public:
    static const uint16_t typeKey = 630;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionValueArtboardComparatorBase::typeKey:
            case TransitionValueIdComparatorBase::typeKey:
            case TransitionValueComparatorBase::typeKey:
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