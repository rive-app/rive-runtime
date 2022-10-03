#include "rive/shapes/mesh_vertex.hpp"
#include "rive/shapes/mesh.hpp"

using namespace rive;
void MeshVertex::markGeometryDirty() { parent()->as<Mesh>()->markDrawableDirty(); }

StatusCode MeshVertex::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    if (!parent()->is<Mesh>())
    {
        return StatusCode::MissingObject;
    }
    parent()->as<Mesh>()->addVertex(this);
    return StatusCode::Ok;
}