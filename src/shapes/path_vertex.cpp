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
#ifndef WITH_RIVE_EDITOR
    // Runtime-only: importer guarantees parent-first hydration so
    // `parent()` is always resolved here. Editor build moves this
    // registration into `editorParentChanged` (called by Pass 4.5
    // of the dispatcher AFTER `onAddedClean` confirms parent is
    // non-null) so coop's intra-batch out-of-order doesn't crash.
    if (!parent()->is<Path>())
    {
        return StatusCode::MissingObject;
    }
    parent()->as<Path>()->addVertex(this);
#endif
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