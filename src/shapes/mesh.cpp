#include "rive/shapes/mesh.hpp"
#include "rive/shapes/image.hpp"
#include "rive/shapes/vertex.hpp"
#include "rive/shapes/mesh_vertex.hpp"
#include "rive/bones/skin.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/span.hpp"
#include "rive/assets/image_asset.hpp"
#include <limits>

using namespace rive;

/// Called whenever a vertex moves (x/y change).
void Mesh::markDrawableDirty()
{
    if (skin() != nullptr)
    {
        skin()->addDirt(ComponentDirt::Skin);
    }

    addDirt(ComponentDirt::Vertices);
}

void Mesh::addVertex(MeshVertex* vertex) { m_Vertices.push_back(vertex); }

StatusCode Mesh::onAddedDirty(CoreContext* context)
{
    StatusCode result = Super::onAddedDirty(context);
    if (result != StatusCode::Ok)
    {
        return result;
    }

    if (!parent()->is<Image>())
    {
        return StatusCode::MissingObject;
    }

    // All good, tell the image it has a mesh.
    parent()->as<Image>()->setMesh(this);

    return StatusCode::Ok;
}

StatusCode Mesh::onAddedClean(CoreContext* context)
{
    // Make sure Core found indices in the file for this Mesh.
    if (m_IndexBuffer == nullptr)
    {
        return StatusCode::InvalidObject;
    }

    // Check the indices are all in range. We should consider having a better
    // error reporting system to the implementor.
    for (auto index : *m_IndexBuffer)
    {
        if (index >= m_Vertices.size())
        {
            return StatusCode::InvalidObject;
        }
    }
    return Super::onAddedClean(context);
}

void Mesh::decodeTriangleIndexBytes(Span<const uint8_t> value)
{
    // decode the triangle index bytes
    rcp<IndexBuffer> buffer = rcp<IndexBuffer>(new IndexBuffer());

    BinaryReader reader(value);
    while (!reader.reachedEnd())
    {
        buffer->push_back(reader.readVarUintAs<uint16_t>());
    }
    m_IndexBuffer = buffer;
}

void Mesh::copyTriangleIndexBytes(const MeshBase& object)
{
    m_IndexBuffer = object.as<Mesh>()->m_IndexBuffer;
}

/// Called whenever a bone moves that is connected to the skin.
void Mesh::markSkinDirty() { addDirt(ComponentDirt::Vertices); }

Core* Mesh::clone() const
{
    auto factory = artboard()->factory();
    auto clone = static_cast<Mesh*>(MeshBase::clone());
    clone->m_VertexRenderBufferDirty = true;
    clone->m_VertexRenderBuffer = factory->makeRenderBuffer(RenderBufferType::vertex,
                                                            RenderBufferFlags::none,
                                                            m_Vertices.size() * sizeof(Vec2D));
    clone->m_UVRenderBuffer = m_UVRenderBuffer;
    clone->m_IndexRenderBuffer = m_IndexRenderBuffer;
    return clone;
}

void Mesh::onAssetLoaded(RenderImage* renderImage)
{
    Mat2D uvTransform = renderImage != nullptr ? renderImage->uvTransform() : Mat2D();

    auto factory = artboard()->factory();
    m_VertexRenderBufferDirty = true;
    m_VertexRenderBuffer = factory->makeRenderBuffer(RenderBufferType::vertex,
                                                     RenderBufferFlags::none,
                                                     m_Vertices.size() * sizeof(Vec2D));

    m_UVRenderBuffer = factory->makeRenderBuffer(RenderBufferType::vertex,
                                                 RenderBufferFlags::mappedOnceAtInitialization,
                                                 m_Vertices.size() * sizeof(Vec2D));
    if (m_UVRenderBuffer)
    {
        float* uv = static_cast<float*>(m_UVRenderBuffer->map());
        for (auto vertex : m_Vertices)
        {
            Vec2D xformedUV = uvTransform * Vec2D(vertex->u(), vertex->v());
            *uv++ = xformedUV.x;
            *uv++ = xformedUV.y;
        }
        m_UVRenderBuffer->unmap();
    }

    m_IndexRenderBuffer = factory->makeRenderBuffer(RenderBufferType::index,
                                                    RenderBufferFlags::mappedOnceAtInitialization,
                                                    m_IndexBuffer->size() * sizeof(uint16_t));
    if (m_IndexRenderBuffer)
    {
        void* indexData = m_IndexRenderBuffer->map();
        memcpy(indexData, m_IndexBuffer->data(), m_IndexRenderBuffer->sizeInBytes());
        m_IndexRenderBuffer->unmap();
    }
}

void Mesh::buildDependencies()
{
    Super::buildDependencies();
    if (skin() != nullptr)
    {
        skin()->addDependent(this);
    }
    parent()->addDependent(this);
}

void Mesh::update(ComponentDirt value)
{
    if (hasDirt(value, ComponentDirt::Vertices))
    {
        if (skin() != nullptr)
        {
            skin()->deform({(Vertex**)m_Vertices.data(), m_Vertices.size()});
        }
        m_VertexRenderBufferDirty = true;
    }
    Super::update(value);
}

void Mesh::draw(Renderer* renderer, const RenderImage* image, BlendMode blendMode, float opacity)
{
    if (m_VertexRenderBufferDirty && m_VertexRenderBuffer != nullptr)
    {
        Vec2D* mappedVertices = reinterpret_cast<Vec2D*>(m_VertexRenderBuffer->map());
        for (auto vertex : m_Vertices)
        {
            *mappedVertices++ = vertex->renderTranslation();
        }
        m_VertexRenderBuffer->unmap();
        m_VertexRenderBufferDirty = false;
    }

    if (skin() == nullptr)
    {
        renderer->transform(parent()->as<WorldTransformComponent>()->worldTransform());
    }
    renderer->drawImageMesh(image,
                            m_VertexRenderBuffer,
                            m_UVRenderBuffer,
                            m_IndexRenderBuffer,
                            static_cast<uint32_t>(m_Vertices.size()),
                            static_cast<uint32_t>(m_IndexBuffer->size()),
                            blendMode,
                            opacity);
}
