#include "rive/layout/axis.hpp"
#include "rive/layout/n_slicer.hpp"

using namespace rive;

StatusCode Axis::onAddedDirty(CoreContext* context)
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

    return StatusCode::Ok;
}

void Axis::offsetChanged() { parent()->as<NSlicer>()->axisChanged(); }
