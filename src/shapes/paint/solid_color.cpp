#include "rive/shapes/paint/solid_color.hpp"
#include "rive/container_component.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/color.hpp"

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
    renderPaint()->color(colorModulateOpacity(colorValue(), renderOpacity()));
}

void SolidColor::applyTo(RenderPaint* renderPaint, float opacityModifier) const
{
    renderPaint->color(colorModulateOpacity(colorValue(), renderOpacity() * opacityModifier));
}

void SolidColor::colorValueChanged() { renderOpacityChanged(); }

bool SolidColor::internalIsTranslucent() const { return colorAlpha(colorValue()) != 0xFF; }
