#include "shapes/paint/linear_gradient.hpp"
#include "renderer.hpp"

using namespace rive;

void LinearGradient::onAddedDirty(CoreContext* context)
{
	Super::onAddedDirty(context);
	assert(initPaintMutator(parent()));
}

void LinearGradient::syncColor()
{
    // renderPaint()->color()
    // sync color
    // 0xFFFFFFFF with opacity of renderOpacity()
}