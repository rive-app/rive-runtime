#include "rive/generated/animation/transition_self_comparator_base.hpp"
#include "rive/animation/transition_self_comparator.hpp"

using namespace rive;

Core* TransitionSelfComparatorBase::clone() const
{
    auto cloned = new TransitionSelfComparator();
    cloned->copy(*this);
    return cloned;
}
