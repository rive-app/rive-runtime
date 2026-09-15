#include "rive/generated/layout/grid_item_placement_base.hpp"
#include "rive/layout/grid_item_placement.hpp"

using namespace rive;

Core* GridItemPlacementBase::clone() const
{
    auto cloned = new GridItemPlacement();
    cloned->copy(*this);
    return cloned;
}
