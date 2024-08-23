#ifndef _RIVE_N_SLICER_TILE_MODE_HPP_
#define _RIVE_N_SLICER_TILE_MODE_HPP_
#include "rive/generated/layout/n_slicer_tile_mode_base.hpp"
#include <stdio.h>
namespace rive
{
class NSlicerTileMode : public NSlicerTileModeBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
};
} // namespace rive

#endif