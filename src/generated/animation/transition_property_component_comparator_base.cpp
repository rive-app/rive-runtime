#include "rive/generated/animation/transition_property_component_comparator_base.hpp"
#include "rive/animation/transition_property_component_comparator.hpp"

using namespace rive;

Core* TransitionPropertyComponentComparatorBase::clone() const
{
    auto cloned = new TransitionPropertyComponentComparator();
    cloned->copy(*this);
    return cloned;
}
