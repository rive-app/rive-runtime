#include "rive/container_component.hpp"
#include "rive/layout/axis_x.hpp"
#include "rive/layout/n_slicer_details.hpp"

using namespace rive;

StatusCode AxisX::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    NSlicerDetails::from(parent())->addAxisX(this);
    return StatusCode::Ok;
}
