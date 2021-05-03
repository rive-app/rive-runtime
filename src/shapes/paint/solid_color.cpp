#include "shapes/paint/solid_color.hpp"
#include "container_component.hpp"
#include "renderer.hpp"
#include "shapes/paint/color.hpp"

using namespace rive;

StatusCode SolidColor::onAddedDirty(CoreContext* context)
{
	StatusCode code = Super::onAddedDirty(context);
	if (code != StatusCode::Ok)
	{
		return code;
	}
	if (!initPaintMutator(this))
	{
		return StatusCode::MissingObject;
	}
	renderOpacityChanged();
	return StatusCode::Ok;
}

void SolidColor::renderOpacityChanged()
{
	if (renderPaint() == nullptr)
	{
		return;
	}
	renderPaint()->color(
	    colorModulateOpacity((unsigned int)colorValue(), renderOpacity()));
}

void SolidColor::colorValueChanged() { renderOpacityChanged(); }