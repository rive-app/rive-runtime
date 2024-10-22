#include "rive/shapes/paint/shape_paint_mutator.hpp"
#include "rive/component.hpp"
#include "rive/shapes/paint/shape_paint.hpp"

using namespace rive;

StatusCode ShapePaintMutator::initPaintMutator(Component* component)
{
    auto parent = component->parent();
    m_Component = component;
    if (parent->is<ShapePaint>())
    {
        if (parent->as<ShapePaint>()->renderPaint() != nullptr)
        {
            DEBUG_PRINT(
                "ShapePaintMutator::initPaintMutator - ShapePaint has already "
                "been asigned a mutator. Does this file have multiple solid "
                "color/gradients in one single fill/stroke?");
            return StatusCode::InvalidObject;
        }
        // Set this object as the mutator for the shape paint and get a
        // reference to the paint we'll be mutating.
        m_RenderPaint = parent->as<ShapePaint>()->initRenderPaint(this);
        return StatusCode::Ok;
    }
    return StatusCode::MissingObject;
}
void ShapePaintMutator::renderOpacity(float value)
{
    if (m_RenderOpacity == value)
    {
        return;
    }
    m_RenderOpacity = value;
    renderOpacityChanged();
}