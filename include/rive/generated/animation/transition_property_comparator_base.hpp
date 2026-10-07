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

    uint16_t coreType() const override { return typeKey; }
};
} // namespace rive

#endif