#include "rive/container_component.hpp"
#include "rive/layout/axis.hpp"
#include "rive/layout/n_slicer_details.hpp"

using namespace rive;

StatusCode Axis::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    if (NSlicerDetails::from(parent()) == nullptr)
    {
        return StatusCode::MissingObject;
    }

    return StatusCode::Ok;
}

void Axis::offsetChanged()
{
    auto details = NSlicerDetails::from(parent());
    if (details != nullptr)
    {
        details->axisChanged();
    }
}
