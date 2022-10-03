#ifndef _RIVE_MESH_VERTEX_HPP_
#define _RIVE_MESH_VERTEX_HPP_
#include "rive/generated/shapes/mesh_vertex_base.hpp"
#include <stdio.h>
namespace rive
{
class MeshVertex : public MeshVertexBase
{
public:
    void markGeometryDirty() override;
    StatusCode onAddedDirty(CoreContext* context) override;
};
} // namespace rive

#endif