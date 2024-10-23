#include "rive/container_component.hpp"
#include "rive/layout/axis_y.hpp"
#include "rive/layout/n_slicer_details.hpp"

using namespace rive;

StatusCode AxisY::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    NSlicerDetails::from(parent())->addAxisY(this);
    return StatusCode::Ok;
}
