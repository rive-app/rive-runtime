#include "rive/drawable.hpp"
#include "rive/artboard.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/shapes/path_composer.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/clip_result.hpp"

using namespace rive;

void Drawable::addClippingShape(ClippingShape* shape) { m_ClippingShapes.push_back(shape); }

ClipResult Drawable::clip(Renderer* renderer) const
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