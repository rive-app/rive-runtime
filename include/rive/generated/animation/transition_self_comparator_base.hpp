#ifndef _RIVE_TRANSITION_SELF_COMPARATOR_BASE_HPP_
#define _RIVE_TRANSITION_SELF_COMPARATOR_BASE_HPP_
#include "rive/animation/transition_comparator.hpp"
namespace rive
{
class TransitionSelfComparatorBase : public TransitionComparator
{
protected:
    typedef TransitionComparator Super;

public:
    static const uint16_t typeKey = 593;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif