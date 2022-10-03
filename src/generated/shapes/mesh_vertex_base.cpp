#include "rive/generated/shapes/mesh_vertex_base.hpp"
#include "rive/shapes/mesh_vertex.hpp"

using namespace rive;

Core* MeshVertexBase::clone() const
{
    auto cloned = new MeshVertex();
    cloned->copy(*this);
    return cloned;
}
