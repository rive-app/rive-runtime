#include "shapes/shape_paint_container.hpp"
#include "artboard.hpp"
#include "component.hpp"
#include "shapes/paint/fill.hpp"
#include "shapes/paint/stroke.hpp"
#include "shapes/shape.hpp"
#include "renderer.hpp"
#include "shapes/metrics_path.hpp"

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
	PathSpace space = m_DefaultPathSpace;
	for (auto paint : m_ShapePaints)
	{
		space |= paint->pathSpace();
	}
	return space;
}

void ShapePaintContainer::invalidateStrokeEffects()
{
	for (auto paint : m_ShapePaints)
	{
		if (paint->is<Stroke>())
		{
			paint->as<Stroke>()->invalidateEffects();
		}
	}
}

RenderPath* ShapePaintContainer::makeRenderPath(PathSpace space)
{
	bool needForRender = false;
	bool needForEffects = false;
	for (auto paint : m_ShapePaints)
	{
		if (space != PathSpace::Neither && space != paint->pathSpace())
		{
			continue;
		}
		if (paint->is<Stroke>() && paint->as<Stroke>()->hasStrokeEffect())
		{
			needForEffects = true;
		}
		else
		{
			needForRender = true;
		}
	}

	if (needForEffects && needForRender)
	{
		return new RenderMetricsPath();
	}
	else if (needForEffects)
	{
		return new OnlyMetricsPath();
	}
	else
	{
		return rive::makeRenderPath();
	}
}