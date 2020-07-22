#include "shapes/paint/shape_paint_mutator.hpp"
#include "component.hpp"
#include "shapes/paint/shape_paint.hpp"

using namespace rive;

bool ShapePaintMutator::initPaintMutator(Component* parent)
{
	if (parent->is<ShapePaint>())
	{
		m_RenderPaint = parent->as<ShapePaint>()->initPaintMutator(this);
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
	syncColor();
}