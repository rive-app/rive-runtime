#include "rive/layout/n_slicer_tile_mode.hpp"
#include "rive/layout/n_slicer.hpp"

using namespace rive;

StatusCode NSlicerTileMode::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    if (!parent()->is<NSlicer>())
    {
        return StatusCode::MissingObject;
    }
    parent()->as<NSlicer>()->addTileMode(patchIndex(), NSlicerTileModeType(style()));
    return StatusCode::Ok;
}
