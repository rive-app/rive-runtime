#ifndef _RIVE_MESH_HPP_
#define _RIVE_MESH_HPP_
#include "rive/generated/shapes/mesh_base.hpp"
#include "rive/bones/skinnable.hpp"
#include "rive/shapes/mesh_drawable.hpp"
#include "rive/span.hpp"
#include "rive/refcnt.hpp"
#include "rive/renderer.hpp"

namespace rive
{
class MeshVertex;

class Mesh : public MeshBase, public Skinnable, public MeshDrawable
{

protected:
    class IndexBuffer : public std::vector<uint16_t>, public RefCnt<IndexBuffer>
    {};
    bool m_VertexRenderBufferDirty = true;
    rcp<IndexBuffer> m_IndexBuffer;
    std::vector<MeshVertex*> m_Vertices;

public:
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;
    void markDrawableDirty();
    void addVertex(MeshVertex* vertex);
    void decodeTriangleIndexBytes(Span<const uint8_t> value) override;
    void copyTriangleIndexBytes(const MeshBase& object) override;
    void buildDependencies() override;
    void update(ComponentDirt value) override;
    void draw(Renderer* renderer,
              const RenderImage* image,
              BlendMode blendMode,
              float opacity) override;

    void markSkinDirty() override;
    Core* clone() const override;

    /// Initialize the any buffers that will be shared amongst instances (the
    /// instance are guaranteed to use the same RenderImage).
    void onAssetLoaded(RenderImage* renderImage) override;

#ifdef TESTING
    std::vector<MeshVertex*>& vertices() { return m_Vertices; }
    rcp<IndexBuffer> indices() { return m_IndexBuffer; }
#endif
};
} // namespace rive

#endif
