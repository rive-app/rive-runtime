#include "rive/generated/animation/transition_property_artboard_comparator_base.hpp"
#include "rive/animation/transition_property_artboard_comparator.hpp"

using namespace rive;

Core* TransitionPropertyArtboardComparatorBase::clone() const
{
    auto cloned = new TransitionPropertyArtboardComparator();
    cloned->copy(*this);
    return cloned;
}
