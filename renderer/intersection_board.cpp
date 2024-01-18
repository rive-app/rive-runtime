/*
 * Copyright 2024 Rive
 */

#include "intersection_board.hpp"

#include "rive/math/math_types.hpp"

namespace rive::pls
{
void IntersectionTile::reset(int left, int top, uint16_t baselineGroupIndex)
{
    m_topLeft = {left, top, left, top};
    m_baselineGroupIndex = baselineGroupIndex;
    m_maxGroupIndex = baselineGroupIndex;
    m_edges.clear();
    m_groupIndices.clear();
    m_rectangleCount = 0;
}

void IntersectionTile::addRectangle(int4 ltrb, uint16_t groupIndex)
{
    assert(simd::all(ltrb.xy < ltrb.zw)); // Ensure ltrb isn't zero or negative.
    // Ensure this rectangle preserves the integrity of our list.
    assert(groupIndex > simd::reduce_max(findMaxIntersectingGroupIndex(ltrb, 0)));
    assert(groupIndex > m_baselineGroupIndex);

    // Translate ltrb to our tile and negate the right and bottom sides.
    ltrb -= m_topLeft;
    ltrb.zw = 255 - ltrb.zw;         // right = 255 - right, bottom = 255 - bottom
    assert(simd::all(ltrb < 255));   // Ensure ltrb isn't completely outside the tile.
    ltrb = simd::max(ltrb, int4(0)); // Clamp ltrb to our tile.

    if (simd::all(ltrb == 0))
    {
        // ltrb covers the entire tile -- reset to a new baseline.
        assert(groupIndex > m_maxGroupIndex);
        reset(m_topLeft.x, m_topLeft.y, groupIndex);
        return;
    }

    // Append a chunk if needed, to make room for this new rectangle.
    uint32_t subIdx = m_rectangleCount % kChunkSize;
    if (subIdx == 0)
    {
        // Push back maximally negative rectangles so they always fail intersection tests.
        assert(m_edges.size() * kChunkSize == m_rectangleCount);
        m_edges.push_back(uint8x32(255));

        // Uninitialized since the corresponding rectangles never pass an intersection test.
        assert(m_groupIndices.size() * kChunkSize == m_rectangleCount);
        m_groupIndices.emplace_back();
    }

    // m_edges is a list of 8 rectangles encoded as [L, T, 255 - R, 255 - B], relative to m_topLeft.
    // The data is also transposed: [L0..L7, T0..T7, -R0..R7, -B0..B7].
    m_edges.back()[subIdx] = ltrb.x;
    m_edges.back()[subIdx + 8] = ltrb.y;
    m_edges.back()[subIdx + 16] = ltrb.z; // Already converted to "255 - right" above.
    m_edges.back()[subIdx + 24] = ltrb.w; // Already converted to "255 - bottom" above.

    m_groupIndices.back()[subIdx] = groupIndex;

    m_maxGroupIndex = std::max(groupIndex, m_maxGroupIndex);
    ++m_rectangleCount;
}

uint16x8 IntersectionTile::findMaxIntersectingGroupIndex(int4 ltrb,
                                                         uint16x8 runningMaxGroupIndices) const
{
    assert(simd::all(ltrb.xy < ltrb.zw)); // Ensure ltrb isn't zero or negative.

    // Translate ltrb to our tile and negate the left and top sides.
    ltrb -= m_topLeft;
    ltrb.xy = 255 - ltrb.xy;           // left = 255 - left, top = 255 - top
    assert(simd::all(ltrb > 0));       // Ensure ltrb isn't completely outside the tile.
    ltrb = simd::min(ltrb, int4(255)); // Clamp ltrb to our tile.

    if (simd::all(ltrb == 255))
    {
        // ltrb covers the entire -- we know it intersects with every rectangle.
        runningMaxGroupIndices[0] = std::max(runningMaxGroupIndices[0], m_maxGroupIndex);
        return runningMaxGroupIndices;
    }

    // Intersection test: l0 < r1 &&
    //                    t0 < b1 &&
    //                    r0 > l1 &&
    //                    b0 > t1
    //
    // Or, to make them all use the same operator: +l0 < +r1 &&
    //                                             +t0 < +b1 &&
    //                                             -r0 < -l1 &&
    //                                             -b0 < -t1
    //
    // m_edges are already encoded like the left column, so encode "ltrb" like the right.
    uint8x8 r = ltrb.z;
    uint8x8 b = ltrb.w;
    uint8x8 _l = ltrb.x; // Already converted to "255 - left" above.
    uint8x8 _t = ltrb.y; // Already converted to "255 - top" above.
    uint8x32 complement = simd::join(r, b, _l, _t);

    auto edges = m_edges.begin();
    auto groupIndices = m_groupIndices.begin();
    assert(m_edges.size() == m_groupIndices.size());
    for (; edges != m_edges.end(); ++edges, ++groupIndices)
    {
        // Test 32 edges!
        auto edgeMasks = *edges < complement;
        // Each byte of isectMasks8 is 0xff if we intersect with the corresponding rectangle,
        // otherwise 0.
        uint64_t isectMasks8 = simd::reduce_and(math::bit_cast<uint64x4>(edgeMasks));
        // Sign-extend isectMasks8 to 16 bits per mask, where each element of isectMasks16 is 0xffff
        // if we intersect with the rectangle, otherwise 0. Encode these as signed integers so the
        // arithmetic shift copies the sign bit.
        int16x4 lo = math::bit_cast<int16x4>(isectMasks8) << 8;
        int16x4 hi = math::bit_cast<int16x4>(isectMasks8);
#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)) || defined(_WIN32)
        int16x8 isectMasks16 = simd::zip(lo, hi) >> 8; // Arithmetic shift right copies sign bit.
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        int16x8 isectMasks16 = simd::zip(hi, lo) >> 8; // Arithmetic shift right copies sign bit.
#endif
        // maskedGroupIndices[j] is m_groupIndices[i][j] if we intersect with the rect, otherwise 0.
        uint16x8 maskedGroupIndices = *groupIndices & isectMasks16;
        runningMaxGroupIndices = simd::max(maskedGroupIndices, runningMaxGroupIndices);
    }

    // Ensure we never drop below our baseline index.
    runningMaxGroupIndices[0] = std::max(runningMaxGroupIndices[0], m_baselineGroupIndex);
    return runningMaxGroupIndices;
}

void IntersectionBoard::resizeAndReset(uint32_t viewportWidth, uint32_t viewportHeight)
{
    m_viewportSize = int2{static_cast<int>(viewportWidth), static_cast<int>(viewportHeight)};

    // Divide the board into 255x255 tiles.
    int2 dims = (m_viewportSize + 254) / 255;
    m_cols = dims.x;
    m_rows = dims.y;
    if (m_tiles.size() < m_cols * m_rows)
    {
        m_tiles.resize(m_cols * m_rows);
    }
    auto tileIter = m_tiles.begin();
    for (int y = 0; y < m_rows; ++y)
    {
        for (int x = 0; x < m_cols; ++x)
        {
            tileIter->reset(x * 255, y * 255);
            ++tileIter;
        }
    }
}

uint16_t IntersectionBoard::addRectangle(int4 ltrb)
{
    // Discard empty, negative, or offscreen rectangles.
    if (simd::any(ltrb.xy >= m_viewportSize || ltrb.zw <= 0 || ltrb.xy >= ltrb.zw))
    {
        return 0;
    }

    // Find the tiled row and column that each corner of the rectangle falls on.
    int4 span = (ltrb - int4{0, 0, 1, 1}) / 255;
    span = simd::clamp(span, int4(0), int4{m_cols, m_rows, m_cols, m_rows} - 1);
    assert(simd::all(span.xy <= span.zw));

    // Accumulate the max groupIndex from each tile the rectangle touches.
    uint16x8 maxGroupIndices = 0;
    for (int y = span.y; y <= span.w; ++y)
    {
        auto tileIter = m_tiles.begin() + y * m_cols + span.x;
        for (int x = span.x; x <= span.z; ++x)
        {
            maxGroupIndices = tileIter->findMaxIntersectingGroupIndex(ltrb, maxGroupIndices);
            ++tileIter;
        }
    }

    // Add the rectangle and its newly-found groupIndex to each tile it touches.
    uint16_t groupIndex = simd::reduce_max(maxGroupIndices) + 1;
    for (int y = span.y; y <= span.w; ++y)
    {
        auto tileIter = m_tiles.begin() + y * m_cols + span.x;
        for (int x = span.x; x <= span.z; ++x)
        {
            tileIter->addRectangle(ltrb, groupIndex);
            ++tileIter;
        }
    }

    return groupIndex;
}
} // namespace rive::pls
