#include "rive/generated/shapes/contour_mesh_vertex_base.hpp"
#include "rive/shapes/contour_mesh_vertex.hpp"

using namespace rive;

Core* ContourMeshVertexBase::clone() const
{
    auto cloned = new ContourMeshVertex();
    cloned->copy(*this);
    return cloned;
}
