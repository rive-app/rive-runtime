#include "rive/shapes/paint/shape_paint_mutator.hpp"
#include "rive/component.hpp"
#include "rive/shapes/paint/shape_paint.hpp"

using namespace rive;

bool ShapePaintMutator::initPaintMutator(Component* component)
{
    auto parent = component->parent();
    m_Component = component;
    if (parent->is<ShapePaint>())
    {
        // Set this object as the mutator for the shape paint and get a
        // reference to the paint we'll be mutating.
        m_RenderPaint = parent->as<ShapePaint>()->initRenderPaint(this);
        return true;
    }
    return false;
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