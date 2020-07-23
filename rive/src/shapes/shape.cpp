#include "shapes/shape.hpp"
#include "shapes/paint/shape_paint.hpp"
#include <algorithm>

using namespace rive;

void Shape::addPath(Path* path)
{
	// Make sure the path is not already in the shape.
	assert(std::find(m_Paths.begin(), m_Paths.end(), path) == m_Paths.end());
	m_Paths.push_back(path);
}

void Shape::update(ComponentDirt value)
{
	Super::update(value);
	
	// RenderOpacity gets updated with the worldTransform (accumulates through
	// hierarchy), so if we see worldTransform is dirty, update our internal
	// render opacities.
	if (hasDirt(value, ComponentDirt::WorldTransform))
	{
		for (auto shapePaint : m_ShapePaints)
		{
			shapePaint->renderOpacity(renderOpacity());
		}
	}
}

void Shape::draw(Renderer* renderer) 
{
	// TODO: ...
}