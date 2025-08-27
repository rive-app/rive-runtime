#include "rive/generated/animation/transition_value_trigger_comparator_base.hpp"
#include "rive/animation/transition_value_trigger_comparator.hpp"

using namespace rive;

Core* TransitionValueTriggerComparatorBase::clone() const
{
    auto cloned = new TransitionValueTriggerComparator();
    cloned->copy(*this);
    return cloned;
}
