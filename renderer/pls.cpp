/*
 * Copyright 2022 Rive
 */

#include "rive/pls/pls.hpp"

namespace rive::pls
{
constexpr static int32_t pack_params(int32_t patchSegmentSpan, int32_t vertexType)
{
    return (patchSegmentSpan << 2) | vertexType;
}

static void generate_buffer_data_for_patch_type(PatchType patchType,
                                                PatchVertex vertices[],
                                                uint16_t indices[],
                                                uint16_t baseVertex)
{
    // AA border vertices. "Inner tessellation curves" have one more segment without a fan triangle
    // whose purpose is to fill the join.
    size_t patchSegmentSpan = patchType == PatchType::midpointFan ? kMidpointFanPatchSegmentSpan
                                                                  : kOuterCurvePatchSegmentSpan;
    size_t vertexCount = 0;
    for (int i = 0; i <= patchSegmentSpan; ++i)
    {
        if (patchType == PatchType::outerCurves)
        {
            vertices[vertexCount++] = {static_cast<float>(i),
                                       1,
                                       0,
                                       pack_params(patchSegmentSpan, flags::kStrokeVertex)};
            vertices[vertexCount++] = {static_cast<float>(i),
                                       0,
                                       .5f,
                                       pack_params(patchSegmentSpan, flags::kStrokeVertex)};
            vertices[vertexCount++] = {static_cast<float>(i),
                                       -1,
                                       0,
                                       pack_params(patchSegmentSpan, flags::kStrokeVertex)};
        }
        else
        {
            assert(patchType == PatchType::midpointFan);
            vertices[vertexCount++] = {static_cast<float>(i),
                                       -1,
                                       1,
                                       pack_params(patchSegmentSpan, flags::kStrokeVertex)};
            vertices[vertexCount++] = {static_cast<float>(i),
                                       1,
                                       0,
                                       pack_params(patchSegmentSpan, flags::kStrokeVertex)};
        }
    }

    // Triangle fan vertices. (These only touch the first "fanSegmentSpan" segments on inner
    // tessellation curves.
    size_t fanSegmentSpan =
        patchType == PatchType::midpointFan ? patchSegmentSpan : patchSegmentSpan - 1;
    assert((fanSegmentSpan & (fanSegmentSpan - 1)) == 0); // The fan must be a power of two.
    size_t fanVerticesIdx = vertexCount;
    for (int i = 0; i <= fanSegmentSpan; ++i)
    {
        vertices[vertexCount++] = {static_cast<float>(i),
                                   patchType == PatchType::outerCurves ? 0.f : -1.f,
                                   1,
                                   pack_params(patchSegmentSpan, flags::kFanVertex)};
    }

    // The midpoint vertex is only included on midpoint fan patches.
    size_t midpointIdx = vertexCount;
    if (patchType == PatchType::midpointFan)
    {
        vertices[vertexCount++] = {0,
                                   0,
                                   1,
                                   pack_params(patchSegmentSpan, flags::kFanMidpointVertex)};
    }
    assert(vertexCount == (patchType == PatchType::outerCurves ? kOuterCurvePatchVertexCount
                                                               : kMidpointFanPatchVertexCount));

    // AA border indices.
    constexpr static size_t kCenterBorderPatternSize = 12;
    constexpr static uint16_t kCenterBorderPattern[kCenterBorderPatternSize] =
        {3, 4, 0, 0, 4, 1, 5, 4, 2, 2, 4, 1};

    constexpr static size_t kOuterBorderPatternSize = 6;
    constexpr static uint16_t kOuterBorderPattern[kOuterBorderPatternSize] = {0, 1, 2, 2, 1, 3};

    size_t borderPatternSize =
        patchType == PatchType::outerCurves ? kCenterBorderPatternSize : kOuterBorderPatternSize;
    const uint16_t* borderPattern =
        patchType == PatchType::outerCurves ? kCenterBorderPattern : kOuterBorderPattern;
    size_t verticesPerNormal = patchType == PatchType::outerCurves ? 3 : 2;
    size_t indexCount = 0;
    for (int i = 0; i < borderPatternSize * patchSegmentSpan; ++i)
    {
        indices[indexCount++] = borderPattern[i % borderPatternSize] +
                                i / borderPatternSize * verticesPerNormal + baseVertex;
    }

    // Triangle fan indices, in a middle-out topology.
    // Don't include the final bowtie join if this is an "outerStroke" patch. (i.e., use
    // fanSegmentSpan and not "patchSegmentSpan".)
    for (int step = 1; step < fanSegmentSpan; step <<= 1)
    {
        for (int i = 0; i < fanSegmentSpan; i += step * 2)
        {
            indices[indexCount++] = fanVerticesIdx + i + baseVertex;
            indices[indexCount++] = fanVerticesIdx + i + step + baseVertex;
            indices[indexCount++] = fanVerticesIdx + i + step * 2 + baseVertex;
        }
    }
    if (patchType == PatchType::midpointFan)
    {
        // Triangle to the contour midpoint.
        indices[indexCount++] = fanVerticesIdx + baseVertex;
        indices[indexCount++] = fanVerticesIdx + fanSegmentSpan + baseVertex;
        indices[indexCount++] = midpointIdx + baseVertex;
        assert(indexCount == kMidpointFanPatchIndexCount);
    }
    else
    {
        assert(patchType == PatchType::outerCurves);
        assert(indexCount == kOuterCurvePatchIndexCount);
    }
}

void GeneratePatchBufferData(PatchVertex vertices[kPatchVertexBufferCount],
                             uint16_t indices[kPatchIndexBufferCount])
{
    generate_buffer_data_for_patch_type(PatchType::midpointFan, vertices, indices, 0);
    generate_buffer_data_for_patch_type(PatchType::outerCurves,
                                        vertices + kMidpointFanPatchVertexCount,
                                        indices + kMidpointFanPatchIndexCount,
                                        kMidpointFanPatchVertexCount);
}

float FindTransformedArea(const AABB& bounds, const Mat2D& matrix)
{
    Vec2D pts[4] = {{bounds.left(), bounds.top()},
                    {bounds.right(), bounds.top()},
                    {bounds.right(), bounds.bottom()},
                    {bounds.left(), bounds.bottom()}};
    Vec2D screenSpacePts[4];
    matrix.mapPoints(screenSpacePts, pts, 4);
    Vec2D v[3] = {screenSpacePts[1] - screenSpacePts[0],
                  screenSpacePts[2] - screenSpacePts[0],
                  screenSpacePts[3] - screenSpacePts[0]};
    return (fabsf(Vec2D::cross(v[0], v[1])) + fabsf(Vec2D::cross(v[1], v[2]))) * .5f;
}
} // namespace rive::pls
