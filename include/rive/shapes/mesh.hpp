#ifndef _RIVE_MESH_HPP_
#define _RIVE_MESH_HPP_
#include "rive/generated/shapes/mesh_base.hpp"
#include "rive/span.hpp"

namespace rive {
    class MeshVertex;
    class Mesh : public MeshBase {
    protected:
        std::vector<MeshVertex*> m_Vertices;

    public:
        StatusCode onAddedDirty(CoreContext* context) override;
        void markDrawableDirty();
        void addVertex(MeshVertex* vertex);
        void decodeTriangleIndexBytes(const Span<uint8_t>& value) override;
        void copyTriangleIndexBytes(const MeshBase& object) override;
#ifdef TESTING
        std::vector<MeshVertex*>& vertices() { return m_Vertices; }
#endif
    };
} // namespace rive

#endif