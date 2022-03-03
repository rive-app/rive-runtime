#include "rive/shapes/mesh.hpp"
#include "rive/shapes/image.hpp"

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

void Mesh::decodeTriangleIndexBytes(const Span<uint8_t>& value) {
    // decode the triangle index bytes
}

void Mesh::copyTriangleIndexBytes(const MeshBase& object) {
    // copy the triangle indices from object
}