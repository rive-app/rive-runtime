#include "shapes/path_vertex.hpp"
#include "shapes/path.hpp"

using namespace rive;

void PathVertex::onAddedDirty(CoreContext* context)
{
	Super::onAddedDirty(context);
	// As asserts for us.
	parent()->as<Path>()->addVertex(this);
}

void PathVertex::markPathDirty()
{
	if (parent() == nullptr)
	{
		// This is an acceptable condition as the parametric paths create points
		// that are not part of the core context.
		return;
	}
	parent()->as<Path>()->markPathDirty();
}