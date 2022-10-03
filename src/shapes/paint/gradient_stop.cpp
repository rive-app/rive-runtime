#include "rive/shapes/paint/gradient_stop.hpp"
#include "rive/shapes/paint/linear_gradient.hpp"

using namespace rive;

StatusCode GradientStop::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    if (!parent()->is<LinearGradient>())
    {
        return StatusCode::MissingObject;
    }
    parent()->as<LinearGradient>()->addStop(this);
    return StatusCode::Ok;
}

void GradientStop::colorValueChanged() { parent()->as<LinearGradient>()->markGradientDirty(); }
void GradientStop::positionChanged() { parent()->as<LinearGradient>()->markStopsDirty(); }