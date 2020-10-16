#include "shapes/paint/shape_paint_mutator.hpp"
#include "component.hpp"
#include "shapes/paint/shape_paint.hpp"

using namespace rive;

bool ShapePaintMutator::initPaintMutator(Component* parent)
{
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