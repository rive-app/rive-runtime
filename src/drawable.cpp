#include "rive/drawable.hpp"
#include "rive/artboard.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/shapes/path_composer.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/clip_result.hpp"

using namespace rive;

StatusCode Drawable::onAddedDirty(CoreContext* context)
{
    auto code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    auto blendMode = static_cast<rive::BlendMode>(blendModeValue());
    switch (blendMode)
    {
        case rive::BlendMode::srcOver:
        case rive::BlendMode::screen:
        case rive::BlendMode::overlay:
        case rive::BlendMode::darken:
        case rive::BlendMode::lighten:
        case rive::BlendMode::colorDodge:
        case rive::BlendMode::colorBurn:
        case rive::BlendMode::hardLight:
        case rive::BlendMode::softLight:
        case rive::BlendMode::difference:
        case rive::BlendMode::exclusion:
        case rive::BlendMode::multiply:
        case rive::BlendMode::hue:
        case rive::BlendMode::saturation:
        case rive::BlendMode::color:
        case rive::BlendMode::luminosity:
            return StatusCode::Ok;
    }
    return StatusCode::InvalidObject;
}

void Drawable::addClippingShape(ClippingShape* shape)
{
    m_ClippingShapes.push_back(shape);
}

ClipResult Drawable::applyClip(Renderer* renderer) const
{
    if (m_ClippingShapes.size() == 0)
    {
        return ClipResult::noClip;
    }

    renderer->save();

    for (auto clippingShape : m_ClippingShapes)
    {
        if (!clippingShape->isVisible())
        {
            continue;
        }

        ShapePaintPath* path = clippingShape->path();
        if (path == nullptr)
        {
            return ClipResult::emptyClip;
        }
        RenderPath* renderPath = path->renderPath(this);
        if (renderPath == nullptr)
        {
            return ClipResult::emptyClip;
        }

        renderer->clipPath(renderPath);
    }
    return ClipResult::clip;
}

bool Drawable::isChildOfLayout(LayoutComponent* layout)
{
    for (ContainerComponent* parent = this; parent != nullptr;
         parent = parent->parent())
    {
        if (parent->is<LayoutComponent>() &&
            parent->as<LayoutComponent>() == layout)
        {
            return true;
        }
    }
    return false;
}

bool Drawable::hitTestPoint(const Vec2D& position,
                            bool skipOnUnclipped,
                            bool isPrimaryHit)
{
    auto hComponent = hittableComponent();
    if (hComponent != this && hComponent != nullptr)
    {
        return hComponent->hitTestPoint(position,
                                        skipOnUnclipped,
                                        isPrimaryHit);
    }
    return Component::hitTestPoint(position, skipOnUnclipped, isPrimaryHit);
}

Drawable* DrawableProxy::hittableComponent()
{
    return static_cast<LayoutComponent*>(proxyDrawing());
}

bool DrawableProxy::isTargetOpaque()
{
    return hittableComponent()->isTargetOpaque();
}