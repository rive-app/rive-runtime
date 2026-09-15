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
              ImageSampler,
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

#ifdef WITH_RIVE_EDITOR
    // Compute triangle indices from `m_Vertices` (UV-space constrained
    // triangulation). Mirrors Dart `Mesh.triangulate()` in
    // `packages/rive_core/lib/shapes/mesh.dart:273-353`. Populates
    // `m_IndexBuffer`. Called from
    // `editor_native::EditorFile::finalizeBatch` for every Mesh after
    // the DAG is sorted.
    //
    // Body intentionally lives in editor_native
    // (`packages/editor_native/native/src/editor/editor_mesh.cpp`)
    // so the triangulation library dependency does not leak into
    // runtime-only builds.
    void triangulateForEditor();

    // Re-allocate the GPU render buffers if the vertex or index count
    // changed since the last allocation. `onAssetLoaded` sizes them
    // against `m_Vertices.size()` / `m_IndexBuffer->size()` at the
    // moment it's called (first asset bind). Subsequent coop batches
    // that add or remove mesh vertices or re-triangulate with a new
    // index count leave the GPU buffers stale, which `Mesh::draw`
    // then walks past — writing off the end of an undersized buffer
    // or leaving garbage at the tail of an oversized one. Call this
    // after `triangulateForEditor` in `finalizeBatch` so the GPU-
    // side picture always matches the CPU-side vertex/index arrays.
    void ensureEditorRenderBuffers();

    // Idempotent variant of `addVertex` for `EditorFile::finalizeBatch`
    // retry. `MeshVertex::onAddedDirty` derefs `parent()->is<Mesh>()`
    // which crashes when the mesh hasn't been hydrated yet (coop
    // intra-batch out-of-order). Same pattern as `LinearGradient
    // ::addStopForEditor` and `Tendon::resolveBone`. No-op if the
    // vertex is already on `m_Vertices` so importer-added vertices
    // in mixed-mode files don't get double-listed.
    void addVertexForEditor(MeshVertex* vertex);
    /// Remove `vertex` from `m_Vertices` if present. Called by
    /// `MeshVertex::editorParentChanged` for unregister / re-parent.
    void removeVertexForEditor(MeshVertex* vertex);

    /// Mesh's own parent transition: registers as the parent
    /// Image's `m_Mesh`. Body in editor_native.
    void editorParentChanged(ContainerComponent* from,
                             ContainerComponent* to) override;
#endif
};
} // namespace rive

#endif
