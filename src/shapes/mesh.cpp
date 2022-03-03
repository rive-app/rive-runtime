#include "rive/shapes/mesh.hpp"
#include "rive/shapes/image.hpp"
#include <limits>
#include <cassert>

using namespace rive;

void Mesh::markDrawableDirty() {
    // TODO: add dirty for rebuilding vertex buffer (including deform).
}

void Mesh::addVertex(MeshVertex* vertex) { m_Vertices.push_back(vertex); }

StatusCode Mesh::onAddedDirty(CoreContext* context) {
    StatusCode result = Super::onAddedDirty(context);
    if (result != StatusCode::Ok) {
        return result;
    }

    if (!parent()->is<Image>()) {
        return StatusCode::MissingObject;
    }
    parent()->as<Image>()->setMesh(this);

    return StatusCode::Ok;
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