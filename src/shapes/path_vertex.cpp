#include "rive/shapes/path_vertex.hpp"
#include "rive/shapes/path.hpp"

using namespace rive;

StatusCode PathVertex::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    if (!parent()->is<Path>())
    {
        return StatusCode::MissingObject;
    }
    parent()->as<Path>()->addVertex(this);
    return StatusCode::Ok;
}

void PathVertex::markGeometryDirty()
{
    if (parent() == nullptr)
    {
        // This is an acceptable condition as the parametric paths create points
        // that are not part of the core context.
        return;
    }
    parent()->as<Path>()->markPathDirty();
}