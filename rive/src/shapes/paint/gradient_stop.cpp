#include "shapes/paint/gradient_stop.hpp"
#include "shapes/paint/linear_gradient.hpp"

using namespace rive;

void GradientStop::onAddedDirty(CoreContext* context)
{
	Super::onAddedDirty(context);
	assert(parent()->is<LinearGradient>());
	parent()->as<LinearGradient>()->addStop(this);
}

void GradientStop::colorValueChanged()
{
	parent()->as<LinearGradient>()->markGradientDirty();
}
void GradientStop::positionChanged()
{
	parent()->as<LinearGradient>()->markStopsDirty();
}