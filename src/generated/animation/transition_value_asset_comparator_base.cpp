#include "rive/generated/animation/transition_value_asset_comparator_base.hpp"
#include "rive/animation/transition_value_asset_comparator.hpp"

using namespace rive;

Core* TransitionValueAssetComparatorBase::clone() const
{
    auto cloned = new TransitionValueAssetComparator();
    cloned->copy(*this);
    return cloned;
}
