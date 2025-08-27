#include "rive/generated/animation/transition_value_boolean_comparator_base.hpp"
#include "rive/animation/transition_value_boolean_comparator.hpp"

using namespace rive;

Core* TransitionValueBooleanComparatorBase::clone() const
{
    auto cloned = new TransitionValueBooleanComparator();
    cloned->copy(*this);
    return cloned;
}
