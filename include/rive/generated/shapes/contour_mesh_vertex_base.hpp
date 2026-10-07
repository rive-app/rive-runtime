#ifndef _RIVE_CONTOUR_MESH_VERTEX_BASE_HPP_
#define _RIVE_CONTOUR_MESH_VERTEX_BASE_HPP_
#include "rive/shapes/mesh_vertex.hpp"
namespace rive
{
class ContourMeshVertexBase : public MeshVertex
{
protected:
    typedef MeshVertex Super;

public:
    static const uint16_t typeKey = 111;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif