#include "rive/drawable.hpp"
#include "rive/artboard.hpp"
#include "rive/layout_component.hpp"
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

void Drawable::addClippingShape(ClippingShape* shape) { m_ClippingShapes.push_back(shape); }

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

        RenderPath* renderPath = clippingShape->renderPath();
        // Can intentionally be null if all the clipping shapes are hidden.
        if (renderPath != nullptr)
        {
            renderer->clipPath(renderPath);
        }
        else
        {
            // If one renderPath is null we exit early because we are treating it
            // as an empty path and its intersection will always be an empty path
            return ClipResult::emptyClip;
        }
    }
    return ClipResult::clip;
}

bool Drawable::isChildOfLayout(LayoutComponent* layout)
{
    for (ContainerComponent* parent = this; parent != nullptr; parent = parent->parent())
    {
        if (parent->is<LayoutComponent>() && parent->as<LayoutComponent>() == layout)
        {
            return true;
        }
    }
    return false;
}