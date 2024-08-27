/*
 * Copyright 2024 Rive
 */

#include "intersection_board.hpp"

#include "rive/math/math_types.hpp"

#if !SIMD_NATIVE_GVEC && (defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64))
// MSVC doesn't get codegen for the inner loop. Provide direct SSE intrinsics.
#include <emmintrin.h>
#define FALLBACK_ON_SSE2_INTRINSICS
#else
#endif

namespace rive::gpu
{
void IntersectionTile::reset(int left, int top, int16_t baselineGroupIndex)
{
    // Since we mask non-intersecting groupIndices to zero, the "mask and max" algorithm is only
    // correct for positive values. (baselineGroupIndex is only signed because SSE doesn't have an
    // unsigned max instruction.)
    assert(baselineGroupIndex >= 0);
    m_topLeft = {left, top, left, top};
    m_baselineGroupIndex = baselineGroupIndex;
    m_maxGroupIndex = baselineGroupIndex;
    m_edges.clear();
    m_groupIndices.clear();
    m_rectangleCount = 0;
}

void IntersectionTile::addRectangle(int4 ltrb, int16_t groupIndex)
{
    assert(simd::all(ltrb.xy < ltrb.zw)); // Ensure ltrb isn't zero or negative.
    // Ensure this rectangle preserves the integrity of our list.
    assert(groupIndex > simd::reduce_max(findMaxIntersectingGroupIndex(ltrb, 0)));
    assert(groupIndex > m_baselineGroupIndex);
    assert(groupIndex >= 0);

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
        m_edges.push_back(int8x32(std::numeric_limits<int8_t>::max()));

        // Uninitialized since the corresponding rectangles never pass an intersection test.
        assert(m_groupIndices.size() * kChunkSize == m_rectangleCount);
        m_groupIndices.emplace_back();
    }

    // m_edges is a list of 8 rectangles encoded as [L, T, 255 - R, 255 - B], relative to m_topLeft.
    // The data is also transposed: [L0..L7, T0..T7, -R0..R7, -B0..B7].
    // Bias ltrb by -128 so we can use int8_t. SSE doesn't have an unsigned byte compare.
    int4 biased = ltrb + std::numeric_limits<int8_t>::min();
    m_edges.back()[subIdx] = biased.x;
    m_edges.back()[subIdx + 8] = biased.y;
    m_edges.back()[subIdx + 16] = biased.z; // Already converted to "255 - right" above.
    m_edges.back()[subIdx + 24] = biased.w; // Already converted to "255 - bottom" above.

    m_groupIndices.back()[subIdx] = groupIndex;

    m_maxGroupIndex = std::max(groupIndex, m_maxGroupIndex);
    ++m_rectangleCount;
}

int16x8 IntersectionTile::findMaxIntersectingGroupIndex(int4 ltrb,
                                                        int16x8 runningMaxGroupIndices) const
{
    assert(simd::all(ltrb.xy < ltrb.zw)); // Ensure ltrb isn't zero or negative.

    // Since we mask non-intersecting groupIndices to zero, the "mask and max" algorithm is only
    // correct for positive values. (runningMaxGroupIndices is only signed because SSE doesn't have
    // an unsigned max instruction.)
    assert(simd::all(runningMaxGroupIndices >= 0));
    assert(m_baselineGroupIndex >= 0);
    assert(m_maxGroupIndex >= m_baselineGroupIndex);

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
    //
    // Bias ltrb by -128 so we can use int8_t. SSE doesn't have an unsigned byte compare.
    int4 biased = ltrb + std::numeric_limits<int8_t>::min();
    int8x8 r = biased.z;
    int8x8 b = biased.w;
    int8x8 _l = biased.x; // Already converted to "255 - left" above.
    int8x8 _t = biased.y; // Already converted to "255 - top" above.

#if !defined(FALLBACK_ON_SSE2_INTRINSICS)
    auto edges = m_edges.begin();
    auto groupIndices = m_groupIndices.begin();
    assert(m_edges.size() == m_groupIndices.size());
    int8x32 complement = simd::join(r, b, _l, _t);
    for (; edges != m_edges.end(); ++edges, ++groupIndices)
    {
        // Test 32 edges!
        auto edgeMasks = *edges < complement;
        // Since the transposed L,T,R,B rows are a each 64-bit vectors, "and-reducing" them returns
        // the intersection test (l0 < r1 && t0 < b1 && r0 > l1 && b0 > t1) in each byte.
        int64_t isectMask = simd::reduce_and(math::bit_cast<int64x4>(edgeMasks));
        // Each element of isectMasks8 is 0xff if we intersect with the corresponding rectangle,
        // otherwise 0.
        int8x8 isectMasks8 = math::bit_cast<int8x8>(isectMask);
        // Widen isectMasks8 to 16 bits per mask, where each element of isectMasks16 is 0xffff if we
        // intersect with the rectangle, otherwise 0.
        int16x8 isectMasks16 = math::bit_cast<int16x8>(simd::zip(isectMasks8, isectMasks8));
        // Mask out any groupIndices we don't intersect with so they don't participate in the test
        // for maximum groupIndex.
        int16x8 maskedGroupIndices = isectMasks16 & *groupIndices;
        runningMaxGroupIndices = simd::max(maskedGroupIndices, runningMaxGroupIndices);
    }
#else
    // MSVC doesn't get good codegen for the above loop. Provide direct SSE intrinsics.
    const __m128i* edgeData = reinterpret_cast<const __m128i*>(m_edges.data());
    const __m128i* groupIndices = reinterpret_cast<const __m128i*>(m_groupIndices.data());
    __m128i complementLO = math::bit_cast<__m128i>(simd::join(r, b));
    __m128i complementHI = math::bit_cast<__m128i>(simd::join(_l, _t));
    __m128i localMaxGroupIndices = math::bit_cast<__m128i>(runningMaxGroupIndices);
    for (size_t i = 0; i < m_groupIndices.size(); ++i)
    {
        __m128i edgesLO = edgeData[i * 2];
        __m128i edgesHI = edgeData[i * 2 + 1];
        // Test 32 edges!
        __m128i edgeMasksLO = _mm_cmpgt_epi8(complementLO, edgesLO);
        __m128i edgeMasksHI = _mm_cmpgt_epi8(complementHI, edgesHI);
        // AND L & R masks (bits 0:63) and T & B masks (bits 63:127).
        __m128i partialIsectMasks = _mm_and_si128(edgeMasksLO, edgeMasksHI);
        // Widen partial edge masks from 8 bits to 16.
        __m128i partialIsectMasksTB16 = _mm_unpackhi_epi8(partialIsectMasks, partialIsectMasks);
        __m128i partialIsectMasksLR16 = _mm_unpacklo_epi8(partialIsectMasks, partialIsectMasks);
        // AND LR masks with TB masks for a full LTRB intersection mask.
        __m128i isectMasks16 = _mm_and_si128(partialIsectMasksLR16, partialIsectMasksTB16);
        // Mask out the groupIndices that don't intersect.
        __m128i intersectingGroupIndices = _mm_and_si128(isectMasks16, groupIndices[i]);
        // Accumulate max intersecting groupIndices.
        localMaxGroupIndices = _mm_max_epi16(intersectingGroupIndices, localMaxGroupIndices);
    }
    runningMaxGroupIndices = math::bit_cast<int16x8>(localMaxGroupIndices);
#endif // !FALLBACK_ON_SSE2_INTRINSICS

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

int16_t IntersectionBoard::addRectangle(int4 ltrb)
{
    // Discard empty, negative, or offscreen rectangles.
    if (simd::any(ltrb.xy >= m_viewportSize || ltrb.zw <= 0 || ltrb.xy >= ltrb.zw))
    {
        return 0;
    }

    // Clamp ltrb to the viewport to avoid integer overflows.
    ltrb.xy = simd::max(ltrb.xy, int2(0));
    ltrb.zw = simd::min(ltrb.zw, m_viewportSize);

    // Find the tiled row and column that each corner of the rectangle falls on.
    int4 span = (ltrb - int4{0, 0, 1, 1}) / 255;
    span = simd::clamp(span, int4(0), int4{m_cols, m_rows, m_cols, m_rows} - 1);
    assert(simd::all(span.xy <= span.zw));

    // Accumulate the max groupIndex from each tile the rectangle touches.
    int16x8 maxGroupIndices = 0;
    for (int y = span.y; y <= span.w; ++y)
    {
        auto tileIter = m_tiles.begin() + y * m_cols + span.x;
        for (int x = span.x; x <= span.z; ++x)
        {
            maxGroupIndices = tileIter->findMaxIntersectingGroupIndex(ltrb, maxGroupIndices);
            ++tileIter;
        }
    }

    // Find the absolute max group index this rectangle intersects with.
    int16_t maxGroupIndex = simd::reduce_max(maxGroupIndices);
    // It is the caller's responsibility to not insert more rectangles than can fit in a signed
    // 16-bit integer.
    assert(maxGroupIndex < std::numeric_limits<int16_t>::max());

    // Add the rectangle and its newly-found groupIndex to each tile it touches.
    int16_t nextGroupIndex = maxGroupIndex + 1;
    for (int y = span.y; y <= span.w; ++y)
    {
        auto tileIter = m_tiles.begin() + y * m_cols + span.x;
        for (int x = span.x; x <= span.z; ++x)
        {
            tileIter->addRectangle(ltrb, nextGroupIndex);
            ++tileIter;
        }
    }

    return nextGroupIndex;
}
} // namespace rive::gpu
