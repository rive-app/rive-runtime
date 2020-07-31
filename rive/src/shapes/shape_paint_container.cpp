#include "shapes/shape_paint_container.hpp"
#include "artboard.hpp"
#include "component.hpp"
#include "shapes/paint/fill.hpp"
#include "shapes/paint/stroke.hpp"
#include "shapes/shape.hpp"

using namespace rive;
ShapePaintContainer* ShapePaintContainer::from(Component* component)
{
	switch (component->coreType())
	{
		case Artboard::typeKey:
			return component->as<Artboard>();
			break;
		case Shape::typeKey:
			return component->as<Shape>();
			break;
	}
	return nullptr;
}

void ShapePaintContainer::addPaint(ShapePaint* paint)
{
	m_ShapePaints.push_back(paint);
}

PathSpace ShapePaintContainer::pathSpace() const
{
	PathSpace space = PathSpace::Neither;
	for (auto paint : m_ShapePaints)
	{
		space |= paint->pathSpace();
	}
	return space;
}