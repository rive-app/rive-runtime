#ifndef _RIVE_MESH_HPP_
#define _RIVE_MESH_HPP_
#include "rive/generated/shapes/mesh_base.hpp"
#include "rive/bones/skinnable.hpp"
#include "rive/span.hpp"
#include "rive/refcnt.hpp"
#include "rive/renderer.hpp"

namespace rive
{
class MeshVertex;

class Mesh : public MeshBase, public Skinnable
{

protected:
    class IndexBuffer : public std::vector<uint16_t>, public RefCnt<IndexBuffer>
    {};
    std::vector<MeshVertex*> m_Vertices;
    rcp<IndexBuffer> m_IndexBuffer;
    bool m_VertexRenderBufferDirty = true;

    rcp<RenderBuffer> m_IndexRenderBuffer;
    rcp<RenderBuffer> m_VertexRenderBuffer;
    rcp<RenderBuffer> m_UVRenderBuffer;

public:
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;
    void markDrawableDirty();
    void addVertex(MeshVertex* vertex);
    void decodeTriangleIndexBytes(Span<const uint8_t> value) override;
    void copyTriangleIndexBytes(const MeshBase& object) override;
    void buildDependencies() override;
    void update(ComponentDirt value) override;
    void draw(Renderer* renderer, const RenderImage* image, BlendMode blendMode, float opacity);

    void updateVertexRenderBuffer(Renderer* renderer);
    void markSkinDirty() override;
    Core* clone() const override;

    /// Initialize the any buffers that will be shared amongst instances (the
    /// instance are guaranteed to use the same RenderImage).
    void initializeSharedBuffers(RenderImage* renderImage);

#ifdef TESTING
    std::vector<MeshVertex*>& vertices() { return m_Vertices; }
    rcp<IndexBuffer> indices() { return m_IndexBuffer; }
#endif
};
} // namespace rive

#endif
