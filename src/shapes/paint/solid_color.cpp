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
    if ((code = initPaintMutator(this)) == StatusCode::Ok)
    {
        renderOpacityChanged();
    }

    return code;
}

void SolidColor::renderOpacityChanged()
{
    if (renderPaint() == nullptr)
    {
        return;
    }
    auto value = colorModulateOpacity(colorValue(), renderOpacity());
    renderPaint()->color(value);
    auto opacity = colorOpacity(value);
    m_flags = ShapePaintMutator::Flags::none;
    if (opacity > 0.0f)
    {
        m_flags |= ShapePaintMutator::Flags::visible;
    }
    else if (opacity < 1.0f)
    {
        m_flags |= ShapePaintMutator::Flags::translucent;
    }
}

void SolidColor::applyTo(RenderPaint* renderPaint, float opacityModifier)
{
    renderPaint->color(
        colorModulateOpacity(colorValue(), renderOpacity() * opacityModifier));
}

void SolidColor::colorValueChanged() { renderOpacityChanged(); }
