#ifndef _RIVE_MESH_HPP_
#define _RIVE_MESH_HPP_
#include "rive/generated/shapes/mesh_base.hpp"

namespace rive {
    class MeshVertex;
    class Mesh : public MeshBase {
    protected:
        std::vector<MeshVertex*> m_Vertices;

    public:
        StatusCode onAddedDirty(CoreContext* context) override;
        void markDrawableDirty();
        void addVertex(MeshVertex* vertex);

#ifdef TESTING
        std::vector<MeshVertex*>& vertices() { return m_Vertices; }
#endif
    };
} // namespace rive

#endif