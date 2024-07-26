#include "rive/generated/animation/transition_value_number_comparator_base.hpp"
#include "rive/animation/transition_value_number_comparator.hpp"

using namespace rive;

Core* TransitionValueNumberComparatorBase::clone() const
{
    auto cloned = new TransitionValueNumberComparator();
    cloned->copy(*this);
    return cloned;
}
