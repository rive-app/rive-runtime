#include "rive/artboard.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/factory.hpp"
#include "rive/layout/axis.hpp"
#include "rive/layout/n_slicer.hpp"
#include "rive/layout/n_slicer_tile_mode.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/n_slicer_helpers.hpp"
#include "rive/shapes/image.hpp"
#include "rive/shapes/slice_mesh.hpp"

using namespace rive;

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
                     ImageSampler ImageSampler,
                     BlendMode blendMode,
                     float opacity)
{
    if (m_nslicer == nullptr || m_nslicer->image() == nullptr)
    {
        return;
    }
    if (!m_VertexRenderBuffer || !m_UVRenderBuffer || !m_IndexRenderBuffer)
    {
        return;
    }
    Image* image = m_nslicer->image();
    renderer->transform(image->worldTransform());
    renderer->translate(-image->width() * image->originX(),
                        -image->height() * image->originY());
    renderer->drawImageMesh(renderImage,
                            ImageSampler,
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
    if (m_VertexRenderBuffer != nullptr &&
        m_VertexRenderBuffer->sizeInBytes() != vertexSizeInBytes)
    {
        m_VertexRenderBuffer = nullptr;
    }

    if (m_VertexRenderBuffer == nullptr && vertexSizeInBytes != 0)
    {
        m_VertexRenderBuffer =
            factory->makeRenderBuffer(RenderBufferType::vertex,
                                      RenderBufferFlags::none, // optimization?
                                      vertexSizeInBytes);
    }

    if (m_VertexRenderBuffer)
    {
        Vec2D* mappedVertices =
            reinterpret_cast<Vec2D*>(m_VertexRenderBuffer->map());
        for (auto v : m_vertices)
        {
            *mappedVertices++ = v;
        }
        m_VertexRenderBuffer->unmap();
    }

    // 2. uv render buffer
    size_t uvSizeInBytes = m_uvs.size() * sizeof(Vec2D);
    if (m_UVRenderBuffer != nullptr &&
        m_UVRenderBuffer->sizeInBytes() != uvSizeInBytes)
    {
        m_UVRenderBuffer = nullptr;
    }

    if (m_UVRenderBuffer == nullptr && uvSizeInBytes != 0)
    {
        m_UVRenderBuffer = factory->makeRenderBuffer(RenderBufferType::vertex,
                                                     RenderBufferFlags::none,
                                                     uvSizeInBytes);
    }

    if (m_UVRenderBuffer)
    {
        auto renderImage = m_nslicer->image()->imageAsset()->renderImage();
        Mat2D uvTransform =
            renderImage != nullptr ? renderImage->uvTransform() : Mat2D();

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
    if (m_IndexRenderBuffer != nullptr &&
        m_IndexRenderBuffer->sizeInBytes() != indexSizeInBytes)
    {
        m_IndexRenderBuffer = nullptr;
    }

    if (m_IndexRenderBuffer == nullptr && indexSizeInBytes != 0)
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
    float imageSize = forAxis == AxisType::X ? m_nslicer->image()->width()
                                             : m_nslicer->image()->height();
    float imageScale =
        std::abs(forAxis == AxisType::X ? m_nslicer->image()->scaleX()
                                        : m_nslicer->image()->scaleY());
    if (imageSize == 0 || imageScale == 0)
    {
        return {};
    }

    const std::vector<Axis*>& axes =
        (forAxis == AxisType::X) ? m_nslicer->xs() : m_nslicer->ys();

    return NSlicerHelpers::uvStops(axes, imageSize);
}

std::vector<float> SliceMesh::vertexStops(
    const std::vector<float>& normalizedStops,
    AxisType forAxis)
{
    Image* image = m_nslicer->image();
    float imageSize = forAxis == AxisType::X ? image->width() : image->height();

    // When doing calcualtions, we assume scale is always non-negative to keep
    // everything in image space.
    float imageScale =
        std::abs(forAxis == AxisType::X ? image->scaleX() : image->scaleY());
    if (imageSize == 0 || imageScale == 0)
    {
        return {};
    }

    ScaleInfo scaleInfo =
        NSlicerHelpers::analyzeUVStops(normalizedStops, imageSize, imageScale);

    std::vector<float> vertices;
    float vertex = 0.0;
    float vertexInBounds = 0.0;

    for (int i = 0; i < (int)normalizedStops.size() - 1; i++)
    {
        vertices.emplace_back(vertexInBounds);
        float segment = imageSize *
                        (normalizedStops[i + 1] - normalizedStops[i]) /
                        imageScale;
        if (NSlicerHelpers::isFixedSegment(i))
        {
            vertex += segment;
        }
        else
        {
            if (scaleInfo.useScale)
            {
                vertex += segment * scaleInfo.scaleFactor;
            }
            else
            {
                vertex += scaleInfo.fallbackSize;
            }
        }
        vertexInBounds = math::clamp(vertex, 0, imageSize);
    }
    vertices.emplace_back(vertexInBounds);
    return vertices;
}

uint16_t SliceMesh::tileRepeat(std::vector<SliceMeshVertex>& vertices,
                               std::vector<uint16_t>& indices,
                               const std::vector<SliceMeshVertex>& box,
                               uint16_t start)
{
    assert(box.size() == 4);

    const float startX = box[0].vertex.x;
    const float startY = box[0].vertex.y;
    const float endX = box[2].vertex.x;
    const float endY = box[2].vertex.y;

    const float startU = box[0].uv.x;
    const float startV = box[0].uv.y;
    const float endU = box[2].uv.x;
    const float endV = box[2].uv.y;

    // The size of each repeated tile in image space
    Image* image = m_nslicer->image();
    float scaleX = std::abs(image->scaleX());
    float scaleY = std::abs(image->scaleY());

    if (scaleX == 0 || scaleY == 0)
    {
        return 0;
    }

    const float sizeX = image->width() * (endU - startU) / scaleX;
    const float sizeY = image->height() * (endV - startV) / scaleY;

    if (std::abs(sizeX) < 1 || std::abs(sizeY) < 1)
    {
        return 0;
    }

    float curX = startX;
    float curY = startY;
    int curV = start;

    int escape = 10000;
    while (curY < endY && escape > 0)
    {
        escape--;
        float fracY = (curY + sizeY) > endY ? (endY - curY) / sizeY : 1;
        curX = startX;
        while (curX < endX && escape > 0)
        {
            escape--;
            int v0 = curV;
            float fracX = (curX + sizeX) > endX ? (endX - curX) / sizeX : 1;

            std::vector<SliceMeshVertex> curTile;
            float endU1 = startU + (endU - startU) * fracX;
            float endV1 = startV + (endV - startV) * fracY;
            float endX1 = curX + sizeX * fracX;
            float endY1 = curY + sizeY * fracY;

            // top left
            SliceMeshVertex v = SliceMeshVertex();
            v.id = curV++;
            v.uv = Vec2D(startU, startV);
            v.vertex = Vec2D(curX, curY);
            curTile.emplace_back(v);

            // top right
            v = SliceMeshVertex();
            v.id = curV++;
            v.uv = Vec2D(endU1, startV);
            v.vertex = Vec2D(endX1, curY);
            curTile.emplace_back(v);

            // bottom right
            v = SliceMeshVertex();
            v.id = curV++;
            v.uv = Vec2D(endU1, endV1);
            v.vertex = Vec2D(endX1, endY1);
            curTile.emplace_back(v);

            // bottom left
            v = SliceMeshVertex();
            v.id = curV++;
            v.uv = Vec2D(startU, endV1);
            v.vertex = Vec2D(curX, endY1);
            curTile.emplace_back(v);

            // Commit the four vertices, and the triangulation
            vertices.insert(vertices.end(), curTile.begin(), curTile.end());
            for (uint16_t t : triangulation)
            {
                indices.emplace_back(v0 + t);
            }

            curX += sizeX;
        }
        curY += sizeY;
    }
    return curV - start;
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
    const auto& tileModes = m_nslicer->tileModes();

    std::vector<SliceMeshVertex> vertices;
    uint16_t vertexIndex = 0;
    for (int patchY = 0; patchY < (int)vs.size() - 1; patchY++)
    {
        for (int patchX = 0; patchX < (int)us.size() - 1; patchX++)
        {
            auto tileModeIt =
                tileModes.find(m_nslicer->patchIndex(patchX, patchY));
            auto tileMode = tileModeIt == tileModes.end()
                                ? NSlicerTileModeType::STRETCH
                                : tileModeIt->second;

            // Do nothing if hidden
            if (tileMode == NSlicerTileModeType::HIDDEN)
            {
                continue;
            }

            const uint16_t v0 = vertexIndex;
            std::vector<SliceMeshVertex> patchVertices;
            for (const Corner& corner : patchCorners)
            {
                int xIndex = patchX + corner.x;
                int yIndex = patchY + corner.y;

                SliceMeshVertex v;
                if (tileMode != NSlicerTileModeType::REPEAT)
                {
                    v.id = vertexIndex++;
                }
                v.uv = Vec2D(us[xIndex], vs[yIndex]);
                v.vertex = Vec2D(xs[xIndex], ys[yIndex]);

                patchVertices.emplace_back(v);
            }

            if (tileMode == NSlicerTileModeType::REPEAT)
            {
                vertexIndex +=
                    tileRepeat(vertices, m_indices, patchVertices, v0);
            }
            else
            {
                vertices.insert(vertices.end(),
                                patchVertices.begin(),
                                patchVertices.end());
                for (uint16_t t : triangulation)
                {
                    m_indices.emplace_back(v0 + t);
                }
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
