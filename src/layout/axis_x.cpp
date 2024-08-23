#include "rive/layout/axis_x.hpp"
#include "rive/layout/n_slicer.hpp"

using namespace rive;

StatusCode AxisX::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    parent()->as<NSlicer>()->addAxisX(this);
    return StatusCode::Ok;
}
