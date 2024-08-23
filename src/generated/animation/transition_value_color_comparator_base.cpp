#include "rive/generated/animation/transition_value_color_comparator_base.hpp"
#include "rive/animation/transition_value_color_comparator.hpp"

using namespace rive;

Core* TransitionValueColorComparatorBase::clone() const
{
    auto cloned = new TransitionValueColorComparator();
    cloned->copy(*this);
    return cloned;
}
