#include "shapes/paint/solid_color.hpp"
#include "container_component.hpp"
#include "renderer.hpp"
#include "shapes/paint/color.hpp"

using namespace rive;

void SolidColor::onAddedDirty(CoreContext* context)
{
	Super::onAddedDirty(context);
	assert(initPaintMutator(parent()));
	renderOpacityChanged();
}

void SolidColor::renderOpacityChanged()
{
	if (renderPaint() == nullptr)
	{
		return;
	}
	renderPaint()->color(colorModulateOpacity((unsigned int)colorValue(), renderOpacity()));
}

void SolidColor::colorValueChanged() { renderOpacityChanged(); }