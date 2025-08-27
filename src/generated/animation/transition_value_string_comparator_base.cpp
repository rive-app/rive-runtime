#include "rive/generated/animation/transition_value_string_comparator_base.hpp"
#include "rive/animation/transition_value_string_comparator.hpp"

using namespace rive;

Core* TransitionValueStringComparatorBase::clone() const
{
    auto cloned = new TransitionValueStringComparator();
    cloned->copy(*this);
    return cloned;
}
