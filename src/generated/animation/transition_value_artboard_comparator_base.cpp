#include "rive/generated/animation/transition_value_artboard_comparator_base.hpp"
#include "rive/animation/transition_value_artboard_comparator.hpp"

using namespace rive;

Core* TransitionValueArtboardComparatorBase::clone() const
{
    auto cloned = new TransitionValueArtboardComparator();
    cloned->copy(*this);
    return cloned;
}
