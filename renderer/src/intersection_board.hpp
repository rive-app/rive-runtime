/*
 * Copyright 2024 Rive
 */

#pragma once

#include "rive/math/simd.hpp"
#include <vector>

namespace rive::gpu
{
// 255 x 255 tile that manages a set of rectangles and their groupIndex.
// From a given rectangle, finds the max groupIndex in the set of internal rectangles it intersects.
// The size is 255 so we can store bounding box coordinates in 8 bits.
class IntersectionTile
{
public:
    void reset(int left, int top, int16_t baselineGroupIndex = 0);

    void addRectangle(int4 ltrb, int16_t groupIndex);

    // Accumulate local maximum intersecting group indices for the given rectangle in each channel
    // of a int16x8.
    // "runningMaxGroupIndices" is a running set of local maximums if the IntersectionBoard also ran
    // this same test on other tile(s) that the rectangle touched.
    // The absolute maximum group index that this rectangle intersects with will be
    // simd::reduce_max(returnValue).
    int16x8 findMaxIntersectingGroupIndex(int4 ltrb, int16x8 runningMaxGroupIndices) const;

private:
    int4 m_topLeft;
    int16_t m_baselineGroupIndex;
    int16_t m_maxGroupIndex;
    size_t m_rectangleCount = 0;

    // How many rectangles/groupIndices are in each chunk of data?
    constexpr static size_t kChunkSize = 8;

    // Chunk of 8 rectangles encoded as [L, T, 255 - R, 255 - B], relative to m_left and m_top.
    // The data is also transposed: [L0..L7, T0..T7, -R0..R7, -B0..B7].
    std::vector<int8x32> m_edges;
    static_assert(sizeof(m_edges[0]) == kChunkSize * 4);

    // Chunk of 8 groupIndices corresponding to the above edges.
    std::vector<int16x8> m_groupIndices;
    static_assert(sizeof(m_groupIndices[0]) == kChunkSize * 2);
};

// Manages a set of rectangles and their groupIndex across a variable-sized viewport.
// Each time a rectangle is added, assigns and returns a groupIndex that is one larger than the max
// groupIndex in the set of existing rectangles it intersects.
class IntersectionBoard
{
public:
    void resizeAndReset(uint32_t viewportWidth, uint32_t viewportHeight);

    // Adds a rectangle to the internal set and assigns it a groupIndex that is one larger than the
    // max groupIndex in the set of existing rectangles it intersects.
    //
    // Returns the newly assigned groupIndex for the added rectangle.
    // If it does not intersect with any other rectangles, this groupIndex is 1.
    //
    // It is the caller's responsibility to not insert more rectangles than can fit in a signed
    // 16-bit integer. (The result is signed because SSE doesn't have an unsigned max instruction.)
    int16_t addRectangle(int4 ltrb);

private:
    int2 m_viewportSize;
    int32_t m_cols;
    int32_t m_rows;
    std::vector<IntersectionTile> m_tiles;
};
} // namespace rive::gpu
