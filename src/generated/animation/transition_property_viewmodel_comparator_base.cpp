#include "rive/generated/animation/transition_property_viewmodel_comparator_base.hpp"
#include "rive/animation/transition_property_viewmodel_comparator.hpp"

using namespace rive;

Core* TransitionPropertyViewModelComparatorBase::clone() const
{
    auto cloned = new TransitionPropertyViewModelComparator();
    cloned->copy(*this);
    return cloned;
}
