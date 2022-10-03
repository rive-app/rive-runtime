#include "rive/drawable.hpp"
#include "rive/artboard.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/shapes/path_composer.hpp"
#include "rive/shapes/shape.hpp"

using namespace rive;

void Drawable::addClippingShape(ClippingShape* shape) { m_ClippingShapes.push_back(shape); }

bool Drawable::clip(Renderer* renderer) const
{
    if (m_ClippingShapes.size() == 0)
    {
        return false;
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
    }
    return true;
}