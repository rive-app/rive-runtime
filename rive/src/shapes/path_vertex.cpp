#include "shapes/path_vertex.hpp"
#include "shapes/path.hpp"

using namespace rive;

void PathVertex::onAddedDirty(CoreContext* context)
{
	Super::onAddedDirty(context);
    // As asserts for us.
	parent()->as<Path>()->addVertex(this);
}