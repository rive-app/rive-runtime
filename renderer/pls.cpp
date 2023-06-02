/*
 * Copyright 2022 Rive
 */

#include "rive/pls/pls.hpp"

namespace rive::pls
{
void GenerateWedgeTriangles(WedgeVertex vertices[], uint16_t indices[], WedgeType wedgeType)
{
    // AA border vertices.
    size_t vertexCount = 0;
    for (int i = 0; i <= kWedgeSize; ++i)
    {
        if (wedgeType == WedgeType::centerStroke)
        {
            vertices[vertexCount++] = {.localVertexID = static_cast<float>(i),
                                       .outset = 1,
                                       .fillCoverage = 0,
                                       .vertexType = flags::kStrokeVertex};
            vertices[vertexCount++] = {.localVertexID = static_cast<float>(i),
                                       .outset = 0,
                                       .fillCoverage = .5f,
                                       .vertexType = flags::kStrokeVertex};
            vertices[vertexCount++] = {.localVertexID = static_cast<float>(i),
                                       .outset = -1,
                                       .fillCoverage = 0,
                                       .vertexType = flags::kStrokeVertex};
        }
        else
        {
            vertices[vertexCount++] = {.localVertexID = static_cast<float>(i),
                                       .outset = -1,
                                       .fillCoverage = 1,
                                       .vertexType = flags::kStrokeVertex};
            vertices[vertexCount++] = {.localVertexID = static_cast<float>(i),
                                       .outset = 1,
                                       .fillCoverage = 0,
                                       .vertexType = flags::kStrokeVertex};
        }
    }

    // Triangle fan vertices.
    size_t fanVerticesIdx = vertexCount;
    for (int i = 0; i <= kWedgeSize; ++i)
    {
        vertices[vertexCount++] = {.localVertexID = static_cast<float>(i),
                                   .outset = wedgeType == WedgeType::centerStroke ? 0.f : -1.f,
                                   .fillCoverage = 1,
                                   .vertexType = flags::kFanVertex};
    }

    // Midpoint vertex.
    size_t midpointIdx = vertexCount;
    vertices[vertexCount++] = {.localVertexID = 0,
                               .outset = 0,
                               .fillCoverage = 1,
                               .vertexType = flags::kFanMidpointVertex};
    assert(vertexCount == (wedgeType == WedgeType::centerStroke ? kCenterStrokeWedgeVertexCount
                                                                : kOuterStrokeWedgeVertexCount));

    // AA border indices.
    constexpr static size_t kCenterBorderPatternSize = 12;
    constexpr static uint16_t kCenterBorderPattern[kCenterBorderPatternSize] =
        {3, 4, 0, 0, 4, 1, 5, 4, 2, 2, 4, 1};

    constexpr static size_t kOuterBorderPatternSize = 6;
    constexpr static uint16_t kOuterBorderPattern[kOuterBorderPatternSize] = {0, 1, 2, 2, 1, 3};

    size_t borderPatternSize =
        wedgeType == WedgeType::centerStroke ? kCenterBorderPatternSize : kOuterBorderPatternSize;
    const uint16_t* borderPattern =
        wedgeType == WedgeType::centerStroke ? kCenterBorderPattern : kOuterBorderPattern;
    size_t verticesPerNormal = wedgeType == WedgeType::centerStroke ? 3 : 2;
    size_t indexCount = 0;
    for (int i = 0; i < borderPatternSize * kWedgeSize; ++i)
    {
        indices[indexCount++] =
            borderPattern[i % borderPatternSize] + i / borderPatternSize * verticesPerNormal;
    }

    // Triangle fan indices, in a middle-out topology.
    for (int step = 1; step < kWedgeSize; step <<= 1)
    {
        for (int i = 0; i < kWedgeSize; i += step * 2)
        {
            indices[indexCount++] = fanVerticesIdx + i;
            indices[indexCount++] = fanVerticesIdx + i + step;
            indices[indexCount++] = fanVerticesIdx + i + step * 2;
        }
    }
    // Triangle to the contour midpoint.
    indices[indexCount++] = fanVerticesIdx;
    indices[indexCount++] = fanVerticesIdx + kWedgeSize;
    indices[indexCount++] = midpointIdx;
    assert(indexCount == (wedgeType == WedgeType::centerStroke ? kCenterStrokeWedgeIndexCount
                                                               : kOuterStrokeWedgeIndexCount));
}
} // namespace rive::pls
