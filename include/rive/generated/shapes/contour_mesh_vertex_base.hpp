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

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ContourMeshVertexBase::typeKey:
            case MeshVertexBase::typeKey:
            case VertexBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;

protected:
};
} // namespace rive

#endif