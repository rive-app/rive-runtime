#include "rive/generated/animation/transition_value_enum_comparator_base.hpp"
#include "rive/animation/transition_value_enum_comparator.hpp"

using namespace rive;

Core* TransitionValueEnumComparatorBase::clone() const
{
    auto cloned = new TransitionValueEnumComparator();
    cloned->copy(*this);
    return cloned;
}
