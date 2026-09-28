#include "rive/shapes/paint/solid_color.hpp"
#include "rive/drawable.hpp"
#include "rive/layer_mask.hpp"
#include "rive/container_component.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/color.hpp"
#include "rive/artboard.hpp"

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
    if (artboard())
    {
        artboard()->changed();
    }
}

void SolidColor::applyTo(RenderPaint* renderPaint, float opacityModifier)
{
    renderPaint->color(
        colorModulateOpacity(colorValue(), renderOpacity() * opacityModifier));
}

void SolidColor::colorValueChanged()
{
    renderOpacityChanged();

    // A colour edit is the one change to a shape's pixels that raises no
    // ComponentDirt at all: the new colour goes straight into the RenderPaint
    // above, and drawing re-reads it every frame, so nothing needs to update.
    // Everything else that alters pixels does raise dirt --
    // Stroke::thicknessChanged, LinearGradient::markGradientDirty, GradientStop
    // notifying its gradient -- which is how a LayerMask learns its cached
    // raster is stale.
    //
    // Raising dirt here was the obvious fix and does not work. There is no
    // spare ComponentDirt bit (all sixteen are taken, several already aliased),
    // and the one that fits semantically, Paint, is acted on by Stroke::update
    // and LinearGradient::update -- so cascading it made them re-push state
    // they had already pushed, which showed up as changed command streams in
    // nine silver tests. Dirtying this SolidColor instead would reach only the
    // mask, but a SolidColor is not in the artboard's dependency order, so the
    // dirt would never clear, and addDirt early-outs on an already-set bit
    // before notifying anyone: the first colour change would report and every
    // one after it would vanish.
    //
    // So the masks are told directly. Narrow on purpose -- it perturbs no other
    // component's update and no dependency order.
    for (ContainerComponent* c = parent(); c != nullptr; c = c->parent())
    {
        if (!c->is<Drawable>())
        {
            continue;
        }
        Drawable* drawable = c->as<Drawable>();
        // Both directions. layerMasks() is the masks drawn over this shape,
        // whose content canvas holds it; sourceOfLayerMasks() is the masks that
        // rasterize it as coverage, whose mask canvas holds it. The second is a
        // separate list because a mask's source generally sits nowhere near the
        // range it masks -- and missing it meant recolouring a mask source kept
        // compositing the old coverage until something else moved.
        for (LayerMask* mask : drawable->layerMasks())
        {
            mask->markRasterDirty();
        }
        for (LayerMask* mask : drawable->sourceOfLayerMasks())
        {
            mask->markRasterDirty();
        }
        break;
    }
}
