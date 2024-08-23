#include "rive/layout/axis_y.hpp"
#include "rive/layout/n_slicer.hpp"

using namespace rive;

StatusCode AxisY::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    parent()->as<NSlicer>()->addAxisY(this);
    return StatusCode::Ok;
}
