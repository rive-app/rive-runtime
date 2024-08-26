#ifndef _RIVE_N_SLICER_TILE_MODE_HPP_
#define _RIVE_N_SLICER_TILE_MODE_HPP_
#include "rive/generated/layout/n_slicer_tile_mode_base.hpp"
#include <stdio.h>
namespace rive
{
enum class NSlicerTileModeType : int
{
    STRETCH = 0,
    REPEAT = 1,
    HIDDEN = 2,
};

class NSlicerTileMode : public NSlicerTileModeBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
};
} // namespace rive

#endif