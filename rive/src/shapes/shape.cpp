#include "shapes/shape.hpp"
#include "shapes/paint/blend_mode.hpp"
#include "shapes/paint/shape_paint.hpp"
#include "shapes/path_composer.hpp"
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

	if (hasDirt(value, ComponentDirt::Paint))
	{
		for (auto shapePaint : m_ShapePaints)
		{
			shapePaint->blendMode((BlendMode)blendModeValue());
		}
	}

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

void Shape::pathChanged() { m_PathComposer->addDirt(ComponentDirt::Path); }
void Shape::pathComposer(PathComposer* value) { m_PathComposer = value; }
void Shape::draw(Renderer* renderer)
{
	for (auto shapePaint : m_ShapePaints)
	{
		// TODO: make this efficient.
		renderer->save();
		if (shapePaint->pathSpace() == PathSpace::Local)
		{
			const Mat2D& transform = worldTransform();
			renderer->transform(transform);
		}
		shapePaint->draw(renderer,
		                 shapePaint->pathSpace() == PathSpace::Local
		                     ? m_PathComposer->localPath()
		                     : m_PathComposer->worldPath());
		renderer->restore();
	}
}

void Shape::buildDependencies()
{
	Super::buildDependencies();
	
	// Set the blend mode on all the shape paints. If we ever animate this
	// property, we'll need to update it in the update cycle/mark dirty when the
	// blend mode changes.
	for (auto paint : m_ShapePaints)
	{
		paint->blendMode((BlendMode)blendModeValue());
	}
}