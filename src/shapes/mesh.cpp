#include "rive/shapes/mesh.hpp"
#include "rive/shapes/image.hpp"
#include "rive/shapes/mesh_vertex.hpp"
#include <limits>

using namespace rive;

void Mesh::markDrawableDirty() { m_VertexRenderBuffer = nullptr; }

void Mesh::addVertex(MeshVertex* vertex) { m_Vertices.push_back(vertex); }

StatusCode Mesh::onAddedDirty(CoreContext* context) {
    StatusCode result = Super::onAddedDirty(context);
    if (result != StatusCode::Ok) {
        return result;
    }

    if (!parent()->is<Image>()) {
        return StatusCode::MissingObject;
    }

    // All good, tell the image it has a mesh.
    parent()->as<Image>()->setMesh(this);

    return StatusCode::Ok;
}

StatusCode Mesh::onAddedClean(CoreContext* context) {
    // Make sure Core found indices in the file for this Mesh.
    if (m_IndexBuffer == nullptr) {
        return StatusCode::InvalidObject;
    }

    // Check the indices are all in range. We should consider having a better
    // error reporting system to the implementor.
    for (auto index : *m_IndexBuffer) {
        if (index >= m_Vertices.size()) {
            return StatusCode::InvalidObject;
        }
    }
    return Super::onAddedClean(context);
}

void Mesh::decodeTriangleIndexBytes(Span<const uint8_t> value) {
    // decode the triangle index bytes
    rcp<IndexBuffer> buffer = rcp<IndexBuffer>(new IndexBuffer());

    BinaryReader reader(value);
    while (!reader.reachedEnd()) {
        uint64_t index = reader.readVarUint64();
        assert(index < std::numeric_limits<uint16_t>::max());
        buffer->push_back(index);
    }
    m_IndexBuffer = buffer;
}

void Mesh::copyTriangleIndexBytes(const MeshBase& object) {
    m_IndexBuffer = object.as<Mesh>()->m_IndexBuffer;
}

void Mesh::markSkinDirty() {}

void Mesh::buildDependencies() {
    Super::buildDependencies();

    // TODO: This logic really needs to move to a "intializeGraphics/Renderer"
    // method that is passed a reference to the Renderer.

    // TODO: if this is an instance, share the index and uv buffer from the
    // source. Consider storing an m_SourceMesh.

    std::vector<float> uv = std::vector<float>(m_Vertices.size() * 2);
    std::size_t index = 0;

    for (auto vertex : m_Vertices) {
        uv[index++] = vertex->u();
        uv[index++] = vertex->v();
    }
    m_UVRenderBuffer = makeBufferF32(uv.data(), uv.size());
    m_IndexRenderBuffer =
        makeBufferU16(m_IndexBuffer->data(), m_IndexBuffer->size());
}

void Mesh::draw(Renderer* renderer,
                const RenderImage* image,
                BlendMode blendMode,
                float opacity) {
    if (m_VertexRenderBuffer == nullptr) {

        std::vector<float> vertices = std::vector<float>(m_Vertices.size() * 2);
        std::size_t index = 0;
        for (auto vertex : m_Vertices) {
            vertices[index++] = vertex->x();
            vertices[index++] = vertex->y();
        }
        m_VertexRenderBuffer = makeBufferF32(vertices.data(), vertices.size());
    }

    renderer->drawImageMesh(image,
                            m_VertexRenderBuffer,
                            m_UVRenderBuffer,
                            m_IndexRenderBuffer,
                            blendMode,
                            opacity);
}