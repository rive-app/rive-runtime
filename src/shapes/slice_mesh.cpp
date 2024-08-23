#include "rive/artboard.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/factory.hpp"
#include "rive/layout/axis.hpp"
#include "rive/layout/n_slicer.hpp"
#include "rive/math/math_types.hpp"
#include "rive/shapes/image.hpp"
#include "rive/shapes/slice_mesh.hpp"

using namespace rive;

bool SliceMesh::isFixedSegment(int i) const { return i % 2 == 0; }

const Corner SliceMesh::patchCorners[] = {
    {0, 0},
    {1, 0},
    {1, 1},
    {0, 1},
};

const uint16_t SliceMesh::triangulation[] = {0, 1, 3, 1, 2, 3};

SliceMesh::SliceMesh(NSlicer* nslicer) : m_nslicer(nslicer) {}

void SliceMesh::draw(Renderer* renderer,
                     const RenderImage* renderImage,
                     BlendMode blendMode,
                     float opacity)
{
    if (m_nslicer == nullptr || m_nslicer->image() == nullptr)
    {
        return;
    }
    Image* image = m_nslicer->image();
    renderer->transform(image->worldTransform());
    renderer->translate(-image->width() * image->originX(), -image->height() * image->originY());
    renderer->drawImageMesh(renderImage,
                            m_VertexRenderBuffer,
                            m_UVRenderBuffer,
                            m_IndexRenderBuffer,
                            static_cast<uint32_t>(m_vertices.size()),
                            static_cast<uint32_t>(m_indices.size()),
                            blendMode,
                            opacity);
}

void SliceMesh::onAssetLoaded(RenderImage* renderImage) {}

void SliceMesh::updateBuffers()
{
    auto factory = m_nslicer->artboard()->factory();

    // 1. vertex render buffer
    size_t vertexSizeInBytes = m_vertices.size() * sizeof(Vec2D);
    if (m_VertexRenderBuffer != nullptr && m_VertexRenderBuffer->sizeInBytes() != vertexSizeInBytes)
    {
        m_VertexRenderBuffer = nullptr;
    }

    if (m_VertexRenderBuffer == nullptr)
    {
        m_VertexRenderBuffer = factory->makeRenderBuffer(RenderBufferType::vertex,
                                                         RenderBufferFlags::none, // optimization?
                                                         vertexSizeInBytes);
    }

    if (m_VertexRenderBuffer)
    {
        Vec2D* mappedVertices = reinterpret_cast<Vec2D*>(m_VertexRenderBuffer->map());
        for (auto v : m_vertices)
        {
            *mappedVertices++ = v;
        }
        m_VertexRenderBuffer->unmap();
    }

    // 2. uv render buffer
    size_t uvSizeInBytes = m_uvs.size() * sizeof(Vec2D);
    if (m_UVRenderBuffer != nullptr && m_UVRenderBuffer->sizeInBytes() != uvSizeInBytes)
    {
        m_UVRenderBuffer = nullptr;
    }

    if (m_UVRenderBuffer == nullptr)
    {
        m_UVRenderBuffer = factory->makeRenderBuffer(RenderBufferType::vertex,
                                                     RenderBufferFlags::none,
                                                     uvSizeInBytes);
    }

    if (m_UVRenderBuffer)
    {
        auto renderImage = m_nslicer->image()->imageAsset()->renderImage();
        Mat2D uvTransform = renderImage != nullptr ? renderImage->uvTransform() : Mat2D();

        Vec2D* mappedUVs = reinterpret_cast<Vec2D*>(m_UVRenderBuffer->map());
        for (auto uv : m_uvs)
        {
            Vec2D xformedUV = uvTransform * uv;
            *mappedUVs++ = xformedUV;
        }
        m_UVRenderBuffer->unmap();
    }

    // 3. index render buffer
    size_t indexSizeInBytes = m_indices.size() * sizeof(uint16_t);
    if (m_IndexRenderBuffer != nullptr && m_IndexRenderBuffer->sizeInBytes() != indexSizeInBytes)
    {
        m_IndexRenderBuffer = nullptr;
    }

    if (m_IndexRenderBuffer == nullptr)
    {
        m_IndexRenderBuffer = factory->makeRenderBuffer(RenderBufferType::index,
                                                        RenderBufferFlags::none,
                                                        indexSizeInBytes);
    }

    if (m_IndexRenderBuffer)
    {
        void* mappedIndex = m_IndexRenderBuffer->map();
        memcpy(mappedIndex, m_indices.data(), indexSizeInBytes);
        m_IndexRenderBuffer->unmap();
    }
}

std::vector<float> SliceMesh::uvStops(AxisType forAxis)
{
    const std::vector<Axis*>& axes = (forAxis == AxisType::X) ? m_nslicer->xs() : m_nslicer->ys();
    std::vector<float> result = {0.0};
    for (const Axis* axis : axes)
    {
        if (axis != nullptr && axis->normalized())
        {
            result.emplace_back(math::clamp(axis->offset(), 0, 1));
        }
        else
        {
            float imageSize =
                forAxis == AxisType::X ? m_nslicer->image()->width() : m_nslicer->image()->height();
            imageSize = std::max(1.0f, imageSize);
            result.emplace_back(math::clamp(axis->offset() / imageSize, 0, 1));
        }
    }
    result.emplace_back(1.0);
    std::sort(result.begin(), result.end());

    return result;
}

std::vector<float> SliceMesh::vertexStops(const std::vector<float>& normalizedStops,
                                          AxisType forAxis)
{
    Image* image = m_nslicer->image();
    float imageSize = forAxis == AxisType::X ? image->width() : image->height();
    float imageScale = std::abs(forAxis == AxisType::X ? image->scaleX() : image->scaleY());

    float fixedPct = 0.0;
    for (int i = 0; i < normalizedStops.size() - 1; i++)
    {
        if (isFixedSegment(i))
        {
            fixedPct += normalizedStops[i + 1] - normalizedStops[i];
        }
    }

    float fixedSize = fixedPct * imageSize;
    float scalableSize = std::max(1.0f, imageSize - fixedSize);
    float scale = (imageSize * imageScale - fixedSize) / scalableSize;

    std::vector<float> result;
    float cur = 0.0;
    for (int i = 0; i < normalizedStops.size() - 1; i++)
    {
        result.emplace_back(cur);
        float segmentSize = imageSize * (normalizedStops[i + 1] - normalizedStops[i]);
        cur += segmentSize * (isFixedSegment(i) ? 1 : scale) / imageScale;
    }
    result.emplace_back(cur);
    return result;
}

void SliceMesh::calc()
{
    m_vertices = {};
    m_indices = {};
    m_uvs = {};

    std::vector<float> us = uvStops(AxisType::X);
    std::vector<float> vs = uvStops(AxisType::Y);
    std::vector<float> xs = vertexStops(us, AxisType::X);
    std::vector<float> ys = vertexStops(vs, AxisType::Y);

    std::vector<SliceMeshVertex> vertices;
    uint16_t vertexIndex = 0;
    for (int patchY = 0; patchY < vs.size() - 1; patchY++)
    {
        for (int patchX = 0; patchX < us.size() - 1; patchX++)
        {
            const uint16_t v0 = vertexIndex;
            std::vector<SliceMeshVertex> patchVertices;
            for (const Corner& corner : patchCorners)
            {
                int xIndex = patchX + corner.x;
                int yIndex = patchY + corner.y;

                SliceMeshVertex v;
                v.id = vertexIndex++;
                v.uv = Vec2D(us[xIndex], vs[yIndex]);
                v.vertex = Vec2D(xs[xIndex], ys[yIndex]);

                patchVertices.emplace_back(v);
            }
            vertices.insert(vertices.end(), patchVertices.begin(), patchVertices.end());
            for (uint16_t t : triangulation)
            {
                m_indices.emplace_back(v0 + t);
            }
        }
    }

    for (const SliceMeshVertex& v : vertices)
    {
        m_vertices.emplace_back(v.vertex);
        m_uvs.emplace_back(v.uv);
    }
}

void SliceMesh::update()
{
    // Make sure the image is loaded
    if (m_nslicer == nullptr || m_nslicer->image() == nullptr ||
        m_nslicer->image()->imageAsset() == nullptr)
    {
        return;
    }

    calc();
    updateBuffers();
}
