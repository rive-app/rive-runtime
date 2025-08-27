#include "rive/container_component.hpp"
#include "rive/layout/n_slicer_tile_mode.hpp"
#include "rive/layout/n_slicer_details.hpp"

using namespace rive;

StatusCode NSlicerTileMode::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    NSlicerDetails* container = NSlicerDetails::from(parent());
    if (container == nullptr)
    {
        return StatusCode::MissingObject;
    }

    container->addTileMode(patchIndex(), NSlicerTileModeType(style()));
    return StatusCode::Ok;
}
