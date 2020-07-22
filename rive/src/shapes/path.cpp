#include "shapes/path.hpp"
#include "renderer.hpp"
#include "shapes/shape.hpp"

using namespace rive;

Path::~Path() { delete m_RenderPath; }

void Path::onAddedDirty(CoreContext* context)
{
	Super::onAddedDirty(context);
	m_RenderPath = makeRenderPath();
}

void Path::onAddedClean(CoreContext* context)
{
	Super::onAddedClean(context);

	// Find the shape.
	for (auto currentParent = parent(); currentParent != nullptr; currentParent = currentParent->parent())
	{
		if (currentParent->is<Shape>())
		{
			m_Shape = currentParent->as<Shape>();
			m_Shape->addPath(this);
			return;
		}
	}
}

const Mat2D& Path::pathTransform() const
{
	// If we're bound to bounds, return identity as points will already be in
	// world space.
	return worldTransform();
}

void Path::update(ComponentDirt value)
{
	if (hasDirt(value, ComponentDirt::Path))
	{
		// TODO: build the render path...
		m_RenderPath->reset();
	}
}