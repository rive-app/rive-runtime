#include "rive/shapes/mesh_vertex.hpp"
#include "rive/shapes/mesh.hpp"

using namespace rive;
void MeshVertex::markGeometryDirty()
{
    parent()->as<Mesh>()->markDrawableDirty();
}

StatusCode MeshVertex::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
#ifndef WITH_RIVE_EDITOR
    // Runtime-only: importer guarantees parent is resolved here.
    // Editor build moves the registration to
    // `editorParentChanged`, dispatched by Pass 4.5 of the
    // dispatcher (after `onAddedClean` confirms parent is non-null).
    if (!parent()->is<Mesh>())
    {
        return StatusCode::MissingObject;
    }
    parent()->as<Mesh>()->addVertex(this);
#endif
    return StatusCode::Ok;
}