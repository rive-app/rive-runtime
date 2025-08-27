#include "rive/generated/layout/n_slicer_tile_mode_base.hpp"
#include "rive/layout/n_slicer_tile_mode.hpp"

using namespace rive;

Core* NSlicerTileModeBase::clone() const
{
    auto cloned = new NSlicerTileMode();
    cloned->copy(*this);
    return cloned;
}
