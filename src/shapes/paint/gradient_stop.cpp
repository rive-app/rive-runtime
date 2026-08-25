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

#ifndef WITH_RIVE_EDITOR
    // Runtime-only path; editor build registers via
    // `editorParentChanged` (dispatcher Pass 4.5).
    if (!parent()->is<LinearGradient>())
    {
        return StatusCode::MissingObject;
    }
    parent()->as<LinearGradient>()->addStop(this);
#endif
    return StatusCode::Ok;
}

void GradientStop::colorValueChanged()
{
    if (parent() != nullptr && parent()->is<LinearGradient>())
    {
        parent()->as<LinearGradient>()->markGradientDirty();
    }
}
void GradientStop::positionChanged()
{
    if (parent() != nullptr && parent()->is<LinearGradient>())
    {
        parent()->as<LinearGradient>()->markStopsDirty();
    }
}