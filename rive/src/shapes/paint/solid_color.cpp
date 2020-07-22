#include "shapes/paint/solid_color.hpp"
#include "container_component.hpp"
#include "renderer.hpp"

using namespace rive;

void SolidColor::onAddedDirty(CoreContext* context)
{
	Super::onAddedDirty(context);
	assert(initPaintMutator(parent()));
}

void SolidColor::syncColor()
{
	// renderPaint()->color()
	// sync color to render paint
	// colorValue() with renderOpacity
}
