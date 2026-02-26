/*
 * Copyright 2024 Rive
 */

#include "intersection_board.hpp"

#include "rive/math/math_types.hpp"

#if !SIMD_NATIVE_GVEC &&                                                       \
    (defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64))
// MSVC doesn't get codegen for the inner loop. Provide direct SSE intrinsics.
#include <emmintrin.h>
#define FALLBACK_ON_SSE2_INTRINSICS
#else
#endif

namespace rive::gpu
{
void IntersectionTile::reset(int left,
                             int top,
                             int16_t baselineGroupIndex,
                             uint16_t baselineOverlapBits)
{
    // Since we mask non-intersecting groupIndices to zero, the "mask and max"
    // algorithm is only correct for positive values. (baselineGroupIndex is
    // only signed because SSE doesn't have an unsigned max instruction.)
    assert(baselineGroupIndex >= 0);
    m_topLeft = {left, top, left, top};
    m_baselineGroupIndex = baselineGroupIndex;
    m_maxGroupIndex = baselineGroupIndex;
    m_baselineOverlapBits = baselineOverlapBits;
    m_overlapBitsForMaxGroup = baselineOverlapBits;
    m_edges.clear();
    m_groupIndices.clear();
    m_overlapBits.clear();
    m_rectangleCount = 0;
}

template <GroupingType Type>
void IntersectionTile::addRectangle(int4 ltrb,
                                    int16_t groupIndex,
                                    uint16_t currentRectangleOverlapBits)
{
    assert(simd::all(ltrb.xy < ltrb.zw)); // Ensure ltrb isn't zero or negative.
    assert(groupIndex >= 0);

    if constexpr (Type == GroupingType::overlapAllowed)
    {
        if (m_overlapBits.size() != m_groupIndices.size())
        {
            // We did not previously have any overlap bits and now we're about
            // to - initialize the array with zeros (everything already in the
            // intersection tile has zeros as its overlap bits, or else the
            // boardContainsNonzeroOverlapBits flag would have already been
            // set).
            assert(m_overlapBits.empty());
            m_overlapBits.resize(m_groupIndices.size());
        }
    }
    else
    {
        static_assert(Type == GroupingType::disjoint);

        // Shouldn't pass any overlap bits in if we indicated they weren't
        // relevant
        assert(currentRectangleOverlapBits == 0);
    }

#if !defined(NDEBUG)
    // Ensure this rectangle preserves the integrity of our list.
    {
        auto r = findMaxIntersectingGroupIndex<Type>(ltrb, {});
        auto maxGroup = simd::reduce_max(r.maxGroupIndices);
        if constexpr (Type == GroupingType::overlapAllowed)
        {
            // If overlapping is allowed, this rectangle can be in the same
            // group as another it overlaps, but cannot be below it.
            assert(groupIndex >= maxGroup);
            assert(groupIndex >= m_baselineGroupIndex);
        }
        else
        {
            // If no overlapping is allowed, this rectangle cannot be at or
            // below a group it overlaps.
            assert(groupIndex > maxGroup);
            assert(groupIndex > m_baselineGroupIndex);
        }
    }
#endif

    // Translate ltrb to our tile and negate the right and bottom sides.
    ltrb -= m_topLeft;
    // right = TILE_DIM - right, bottom = TILE_DIM- bottom
    ltrb.zw = TILE_DIM - ltrb.zw;
    // Ensure ltrb isn't completely outside the tile.
    assert(simd::all(ltrb < TILE_DIM));
    ltrb = simd::max(ltrb, int4(0)); // Clamp ltrb to our tile.

    if constexpr (Type == GroupingType::overlapAllowed)
    {
        if (groupIndex == m_baselineGroupIndex &&
            (currentRectangleOverlapBits | m_baselineOverlapBits) ==
                m_baselineOverlapBits)
        {
            // Nothing needs to be done here, this is going into the baseline
            // group and it adds no new overlap bits that are not already in the
            // baseline.
            return;
        }
    }

    if (simd::all(ltrb == 0))
    {
        // ltrb covers the entire tile
        if constexpr (Type == GroupingType::overlapAllowed)
        {
            // This rectangle is allowed to be at or higher than the max group
            // (rather than strictly above it, in the no-overlap case)
            assert(groupIndex >= m_maxGroupIndex);

            if (groupIndex == m_maxGroupIndex)
            {
                // This rectangle is within the existing max group, so do a
                // heavier-handed update (some rectangles in the max group
                // may still need to stay, so we can't do a reset)
                updateBaselineToMaxGroupIndex<Type>(
                    currentRectangleOverlapBits);
                return;
            }
        }
        else
        {
            // If no overlapping is allowed, a full-tile rectangle had
            // better be above the current max group.
            assert(groupIndex > m_maxGroupIndex);
        }

        // Setting a new baseline, so reset everything, update the group index
        // and baseline overlap
        reset(m_topLeft.x,
              m_topLeft.y,
              groupIndex,
              currentRectangleOverlapBits);
        return;
    }

    // Append a chunk if needed, to make room for this new rectangle.
    uint32_t subIdx = m_rectangleCount % CHUNK_SIZE;
    if (subIdx == 0)
    {
        // Push back maximally negative rectangles so they always fail
        // intersection tests.
        assert(m_edges.size() * CHUNK_SIZE == m_rectangleCount);
        m_edges.push_back(int8x32(TILE_DIM + TILE_EDGE_BIAS));

        // Uninitialized since the corresponding rectangles never pass an
        // intersection test.
        assert(m_groupIndices.size() * CHUNK_SIZE == m_rectangleCount);
        m_groupIndices.emplace_back();

        if constexpr (Type == GroupingType::overlapAllowed)
        {
            assert(m_overlapBits.size() * CHUNK_SIZE == m_rectangleCount);
            m_overlapBits.emplace_back();
        }
    }

    // m_edges is a list of 8 rectangles encoded as
    // [L, T, TILE_DIM - R, TILE_DIM - B], relative to m_topLeft. The data is
    // also transposed: [L0..L7, T0..T7, -R0..R7, -B0..B7]. Bias ltrb so
    // we can use int8_t. SSE doesn't have an unsigned byte compare.
    int4 biased = ltrb + TILE_EDGE_BIAS;
    m_edges.back()[subIdx] = biased.x;
    m_edges.back()[subIdx + 1 * CHUNK_SIZE] = biased.y;
    m_edges.back()[subIdx + 2 * CHUNK_SIZE] =
        biased.z; // Already converted to "TILE_DIM - right" above.
    m_edges.back()[subIdx + 3 * CHUNK_SIZE] =
        biased.w; // Already converted to "TILE_DIM - bottom" above.

    m_groupIndices.back()[subIdx] = groupIndex;

    if constexpr (Type == GroupingType::overlapAllowed)
    {
        m_overlapBits.back()[subIdx] = currentRectangleOverlapBits;

        if (groupIndex > m_maxGroupIndex)
        {
            // There's a new maximum group, so set it and restart its overlap
            // bits.
            m_maxGroupIndex = groupIndex;
            m_overlapBitsForMaxGroup = currentRectangleOverlapBits;
        }
        else if (groupIndex == m_maxGroupIndex)
        {
            // This is going into the existing max group so add its overlap bits
            // to the max group's tracked overlap bits
            m_overlapBitsForMaxGroup |= currentRectangleOverlapBits;
        }
    }
    else
    {
        // If we aren't tracking overlap bits then simply ensuring the max group
        // index is correct is sufficient.
        m_maxGroupIndex = std::max(m_maxGroupIndex, groupIndex);
    }

    ++m_rectangleCount;
}

#ifdef WITH_RIVE_TOOLS
template <GroupingType Type>
void IntersectionTile::testingOnly_addRectangleAndValidate(
    int4 ltrb,
    int16_t groupIndex,
    uint16_t currentRectangleOverlapBits)
{
    addRectangle<Type>(ltrb, groupIndex, currentRectangleOverlapBits);

    // Baseline should not be higher than the inserted rectangle,
    // max group should not be lower.
    assert(m_baselineGroupIndex <= groupIndex);
    assert(m_maxGroupIndex >= groupIndex);

    // Should be no baseline overlap bits if there's no baseline
    // (ditto for max group)
    assert(m_baselineGroupIndex != 0 || m_baselineOverlapBits == 0);
    assert(m_maxGroupIndex != 0 || m_overlapBitsForMaxGroup == 0);

    uint16_t foundOverlapBitsForMaxGroup;

    if constexpr (Type == GroupingType::overlapAllowed)
    {
        foundOverlapBitsForMaxGroup = (m_baselineGroupIndex == m_maxGroupIndex)
                                          ? m_baselineOverlapBits
                                          : 0u;
    }
    else
    {
        // Ignore this value (not using std::ignore here because it
        // would require initializing the value first, which would
        // mean the compiler won't catch a mistake where this gets
        // used elsewhere when it shouldn't be)
        (void)foundOverlapBitsForMaxGroup;

        // If overlap isn't allowed this should never be allocated
        assert(m_overlapBits.empty());
    }

    // Exhaustively check to ensure that the overlap bits and max
    // group indices match our tracked values.
    auto maxGroupIndexFound = m_baselineGroupIndex;
    for (auto i = 0u; i < m_rectangleCount; i++)
    {
        auto outerI = i / CHUNK_SIZE;
        auto innerI = i % CHUNK_SIZE;

        assert(m_groupIndices[outerI][innerI] >= m_baselineGroupIndex);
        assert(m_groupIndices[outerI][innerI] <= m_maxGroupIndex);
        if constexpr (Type == GroupingType::overlapAllowed)
        {
            if (m_groupIndices[outerI][innerI] == m_maxGroupIndex)
            {
                foundOverlapBitsForMaxGroup |= m_overlapBits[outerI][innerI];
            }
        }
        maxGroupIndexFound =
            std::max(maxGroupIndexFound, m_groupIndices[outerI][innerI]);
    }

    if constexpr (Type == GroupingType::overlapAllowed)
    {
        assert(foundOverlapBitsForMaxGroup == m_overlapBitsForMaxGroup);
        if (m_baselineGroupIndex == m_maxGroupIndex)
        {
            // max group is allowed to have more set bits than the
            // baseline (as there might be non-full-tile rectangles
            // at that level), but there should never be bits in
            // baseline that aren't part of the max group set if
            // their group indices match.
            assert((m_overlapBitsForMaxGroup | m_baselineOverlapBits) ==
                   m_overlapBitsForMaxGroup);
        }
    }
    else
    {
        assert(m_overlapBitsForMaxGroup == 0);
        assert(m_baselineOverlapBits == 0);
    }

    assert(maxGroupIndexFound == m_maxGroupIndex);
}
#endif

template <GroupingType Type>
void IntersectionTile::updateBaselineToMaxGroupIndex(
    uint16_t additionalBaselineOverlapBits)
{
    // This should only be called when overlap testing is enabled. The template
    // argument is really just here to make it hard to accidentally call this
    // from a place where it will be wrong
    static_assert(Type == GroupingType::overlapAllowed);

    // We've added a full-tile rectangle at m_maxGroupIndex, which means we can
    // update the baseline and its overlap bits.
    if ((additionalBaselineOverlapBits | m_overlapBitsForMaxGroup) ==
            additionalBaselineOverlapBits ||
        m_rectangleCount == 0)
    {
        // This rectangle both covers the whole tile and either it contains all
        // of the existing overlap bits in this group, or there are no
        // existing rectangles (the max group is the already just the baseline),
        // which means we can do a simple reset, as we don't need to keep any
        // rectangles - even ones that are in this max group.
        reset(m_topLeft.x,
              m_topLeft.y,
              m_maxGroupIndex,
              m_overlapBitsForMaxGroup | additionalBaselineOverlapBits);
        return;
    }

    m_overlapBitsForMaxGroup |= additionalBaselineOverlapBits;

    if (m_maxGroupIndex == m_baselineGroupIndex)
    {
        if (m_overlapBitsForMaxGroup == m_baselineOverlapBits)
        {
            // This rectangle does not change the baseline or its bits in
            // any way, so we're done early.
            return;
        }

        m_baselineOverlapBits |= additionalBaselineOverlapBits;
    }
    else
    {
        m_baselineGroupIndex = m_maxGroupIndex;
        m_baselineOverlapBits = additionalBaselineOverlapBits;
    }

    // Do a pass through the array and zero out the group indices of any
    // rectangles that should no longer remain in the list (this makes the test
    // in the compaction loop more efficient).
    {
#if !defined(FALLBACK_ON_SSE2_INTRINSICS)
        auto baselineGroupIndexVector = int16x8(m_baselineGroupIndex);
        auto baselineOverlapBitsVector = uint16x8(m_baselineOverlapBits);
#else
        auto baselineGroupIndexVector = _mm_set1_epi16(m_baselineGroupIndex);
        auto baselineOverlapBitsVector = _mm_set1_epi16(m_baselineOverlapBits);
#endif
        for (size_t i = 0; i < m_groupIndices.size(); i++)
        {
#if !defined(FALLBACK_ON_SSE2_INTRINSICS)
            // Groups under the baseline will go away
            auto mask = m_groupIndices[i] >= baselineGroupIndexVector;

            // Of those that remained, only keep the ones where the overlap bits
            // have bits that aren't in the baseline's bits.
            mask &= ((baselineOverlapBitsVector | m_overlapBits[i]) !=
                     baselineOverlapBitsVector);

            m_groupIndices[i] &= mask;
#else
            // The SSE2 code has to build the mask backwards from the above
            // code, as there is neither a ">=" or "!=" operator.
            auto groupIndices = math::bit_cast<__m128i>(m_groupIndices[i]);
            auto overlapBits = math::bit_cast<__m128i>(m_overlapBits[i]);
            auto mask = _mm_cmpgt_epi16(baselineGroupIndexVector, groupIndices);

            mask = _mm_or_si128(
                mask,
                _mm_cmpeq_epi16(
                    baselineOverlapBitsVector,
                    _mm_or_si128(baselineOverlapBitsVector, overlapBits)));

            // Conveniently, there is a (~mask & groupIndices) single operation
            // to allow us to use the inverted mask directly.
            groupIndices = _mm_andnot_si128(mask, groupIndices);
            m_groupIndices[i] = math::bit_cast<int16x8>(groupIndices);
#endif
        }
    }

    // Now, remove all of the rectangles that are no longer above the line.
    // (Thankfully, ordering doesn't matter, so we don't need the partitioning
    // to be stable)

    // Start by scaning the first index forward until we find a rectangle that
    // doesn't need to stay (or we hit the end)
    size_t firstIdx = 0u;
    while (m_groupIndices[firstIdx / CHUNK_SIZE][firstIdx % CHUNK_SIZE] != 0)
    {
        firstIdx++;
        if (firstIdx == m_rectangleCount)
        {
            // Every rectangle stays! We don't have to do any additional
            // work.
            return;
        }
    }

    size_t lastIdx = m_rectangleCount - 1;
    while (lastIdx > firstIdx &&
           m_groupIndices[lastIdx / CHUNK_SIZE][lastIdx % CHUNK_SIZE] == 0)
    {
        --lastIdx;
    }

    while (firstIdx < lastIdx)
    {
        // Now first index points to something that should go and last index
        // points to something that should stay, and they're out of order. Move
        // the latter element to the beginning to put it into place.
        {
            size_t firstOuterIdx = firstIdx / CHUNK_SIZE;
            size_t firstInnerIdx = firstIdx % CHUNK_SIZE;
            size_t lastOuterIdx = lastIdx / CHUNK_SIZE;
            size_t lastInnerIdx = lastIdx % CHUNK_SIZE;

            // Put the one that wants to stay at the first index.

            m_edges[firstOuterIdx][firstInnerIdx + 0] =
                m_edges[lastOuterIdx][lastInnerIdx + 0];
            m_edges[firstOuterIdx][firstInnerIdx + CHUNK_SIZE] =
                m_edges[lastOuterIdx][lastInnerIdx + CHUNK_SIZE];
            m_edges[firstOuterIdx][firstInnerIdx + 2 * CHUNK_SIZE] =
                m_edges[lastOuterIdx][lastInnerIdx + 2 * CHUNK_SIZE];
            m_edges[firstOuterIdx][firstInnerIdx + 3 * CHUNK_SIZE] =
                m_edges[lastOuterIdx][lastInnerIdx + 3 * CHUNK_SIZE];

            m_groupIndices[firstOuterIdx][firstInnerIdx] =
                m_groupIndices[lastOuterIdx][lastInnerIdx];
            m_overlapBits[firstOuterIdx][firstInnerIdx] =
                m_overlapBits[lastOuterIdx][lastInnerIdx];

            // Update the group index at the last index to be a 0, which now
            // marks it as a "this doesn't need to stay" rectangle. This allows
            // it to act as a sentinel value for the loop where we increment
            // firstRectangleIndex so it can avoid an extra bounds check.
            m_groupIndices[lastOuterIdx][lastInnerIdx] = 0;
        }

        // Because of that zero, we're now guaranteed that there
        // is a rectangle that should be removed to the right of the first index
        // and one that should stay to the left of the last index (minimally,
        // the ones we just adjusted), so these inner loops do not need to
        // bounds check.
        do
        {
            ++firstIdx;
            assert(firstIdx < m_rectangleCount);
        } while (m_groupIndices[firstIdx / CHUNK_SIZE][firstIdx % CHUNK_SIZE] !=
                 0);

        do
        {
            --lastIdx;
            // NOTE: This assert seems backwards, but it's an unsigned variable
            // and will wrap past zero, and thus go *very* positive.
            assert(lastIdx < m_rectangleCount);
        } while (m_groupIndices[lastIdx / CHUNK_SIZE][lastIdx % CHUNK_SIZE] ==
                 0);
    }

    // firstRectangleIndex now points to the first rectangle that needs to be
    // removed, which means it is also our new rectangle count.
    m_rectangleCount = firstIdx;
    auto vectorElementCount = (m_rectangleCount + CHUNK_SIZE - 1) / CHUNK_SIZE;
    m_edges.resize(vectorElementCount);
    m_groupIndices.resize(vectorElementCount);
    m_overlapBits.resize(vectorElementCount);

    if (vectorElementCount > 0 && (m_rectangleCount % CHUNK_SIZE) != 0)
    {
        auto& lastEdges = m_edges.back();
        auto& lastGroupIndices = m_groupIndices.back();
        auto& lastOverlapBits = m_overlapBits.back();

        constexpr auto DEFAULT_EDGE_EXTENT = TILE_DIM + TILE_EDGE_BIAS;

        // We have a chunk with potentially some boxes that had entries that
        // shouldn't anymore, so clear them.
        for (auto i = (m_rectangleCount % CHUNK_SIZE); i < CHUNK_SIZE; i++)
        {
            lastEdges[i + 0 * CHUNK_SIZE] = DEFAULT_EDGE_EXTENT;
            lastEdges[i + 1 * CHUNK_SIZE] = DEFAULT_EDGE_EXTENT;
            lastEdges[i + 2 * CHUNK_SIZE] = DEFAULT_EDGE_EXTENT;
            lastEdges[i + 3 * CHUNK_SIZE] = DEFAULT_EDGE_EXTENT;
            lastGroupIndices[i] = 0;
            lastOverlapBits[i] = 0;
        }
    }
}

template <GroupingType Type>
IntersectionTile::FindResult IntersectionTile::findMaxIntersectingGroupIndex(
    int4 ltrb,
    FindResult running) const
{
    assert(simd::all(ltrb.xy < ltrb.zw)); // Ensure ltrb isn't zero or negative.

    // Since we mask non-intersecting groupIndices to zero, the "mask and max"
    // algorithm is only correct for positive values. (running.maxGroupIndices
    // is only signed because SSE pre 4.1 doesn't have an unsigned max
    // instruction.)
    assert(simd::all(running.maxGroupIndices >= 0));
    assert(m_baselineGroupIndex >= 0);
    assert(m_maxGroupIndex >= m_baselineGroupIndex);

    // Translate ltrb to our tile and negate the left and top sides.
    ltrb -= m_topLeft;
    // left = TILE_DIM - left, top = TILE_DIM - top
    ltrb.xy = TILE_DIM - ltrb.xy;
    assert(
        simd::all(ltrb > 0)); // Ensure ltrb isn't completely outside the tile.
    ltrb = simd::min(ltrb, int4(TILE_DIM)); // Clamp ltrb to our tile.

    if (simd::all(ltrb == TILE_DIM))
    {
        // ltrb covers the tile -- we know it intersects with every rectangle.
        if constexpr (Type == GroupingType::overlapAllowed)
        {
            auto currentMax = running.maxGroupIndices[0];
            if (currentMax < m_maxGroupIndex)
            {
                // The first element's max group index is changing, so restart
                // its overlap bits with the new set.
                running.maxGroupIndices[0] = m_maxGroupIndex;
                running.overlapBits[0] = m_overlapBitsForMaxGroup;
            }
            else if (currentMax == m_maxGroupIndex)
            {
                // The group index of this element is the same, so just add in
                // any new overlap bits that might exist.
                running.overlapBits[0] |= m_overlapBitsForMaxGroup;
            }
        }
        else
        {
            static_assert(Type == GroupingType::disjoint);

            running.maxGroupIndices[0] =
                std::max(running.maxGroupIndices[0], m_maxGroupIndex);
        }

        return running;
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
    // m_edges are already encoded like the left column, so encode "ltrb" like
    // the right.
    //
    // Bias ltrb so we can use int8_t. SSE (pre-4.1) doesn't have an unsigned
    // byte compare.
    int4 biased = ltrb + TILE_EDGE_BIAS;
    int8x8 r = biased.z;
    int8x8 b = biased.w;
    int8x8 _l = biased.x; // Already converted to "TILE_DIM - left" above.
    int8x8 _t = biased.y; // Already converted to "TILE_DIM - top" above.

    assert(m_edges.size() == m_groupIndices.size());
    if constexpr (Type == GroupingType::overlapAllowed)
    {
        assert(m_edges.size() == m_overlapBits.size());
    }

#if !defined(FALLBACK_ON_SSE2_INTRINSICS)
    auto edges = m_edges.begin();
    auto groupIndices = m_groupIndices.begin();
    const uint16x8* groupOverlapBits = nullptr;

    // We only care about groupOverlapBits if this rectangle overlap testing is
    // enabled *and* there are non-zero overlap bits in the mix
    if constexpr (Type == GroupingType::overlapAllowed)
    {
        groupOverlapBits = m_overlapBits.data();
    }
    PUSH_DISABLE_CLANG_SIMD_ABI_WARNING()
    int8x32 complement = simd::join(r, b, _l, _t);
    for (; edges != m_edges.end(); ++edges, ++groupIndices)
    {
        // Test 32 edges!
        auto edgeMasks = *edges < complement;
        // Since the transposed L,T,R,B rows are a each 64-bit vectors,
        // "and-reducing" them returns the intersection test (l0 < r1 && t0 < b1
        // && r0 > l1 && b0 > t1) in each byte.
        int64_t isectMask =
            simd::reduce_and(math::bit_cast<int64x4>(edgeMasks));
        // Each element of isectMasks8 is 0xff if we intersect with the
        // corresponding rectangle, otherwise 0.
        int8x8 isectMasks8 = math::bit_cast<int8x8>(isectMask);
        // Widen isectMasks8 to 16 bits per mask, where each element of
        // isectMasks16 is 0xffff if we intersect with the rectangle, otherwise
        // 0.
        int16x8 isectMasks16 =
            math::bit_cast<int16x8>(simd::zip(isectMasks8, isectMasks8));
        // Mask out any groupIndices we don't intersect with so they don't
        // participate in the test for maximum groupIndex.
        int16x8 maskedGroupIndices = isectMasks16 & *groupIndices;

        // Doing this as a compile-time check to not have to do the test in the
        // loop at runtime.
        if constexpr (Type == GroupingType::overlapAllowed)
        {
            // Clear any bits from the running overlap where the new index is
            // covering it (i.e. there's a new closest group index)
            auto keepRunningBits =
                (maskedGroupIndices <= running.maxGroupIndices);
            running.overlapBits &= keepRunningBits;

            // Keep any bits from the current group where the running index is
            // not closer (i.e. where current is not covered).
            // Note: this will additionally keep bits where there has not yet
            // been an intersection (index is 0), and the current rectangle was
            // not intersected. This is fine - these bits will get cleared by
            // the above code when a rectangle intersection finally *does*
            // happen, so we can save on an additional mask with isectMask16.
            auto keepCurrentBits =
                (running.maxGroupIndices <= maskedGroupIndices);
            auto maskedOverlapBits = *groupOverlapBits & keepCurrentBits;

            // Combine the two sets, ultimately this will add bits from
            // overlapping rectangles that are in the new-highest draw group and
            // clear any bits that are not.
            running.overlapBits |= maskedOverlapBits;

            ++groupOverlapBits;
        }

        running.maxGroupIndices =
            simd::max(maskedGroupIndices, running.maxGroupIndices);
    }
    POP_DISABLE_CLANG_SIMD_ABI_WARNING()

#else
    // MSVC doesn't get good codegen for the above loop. Provide direct SSE
    // intrinsics.
    const __m128i* edgeData = reinterpret_cast<const __m128i*>(m_edges.data());
    const __m128i* groupIndices =
        reinterpret_cast<const __m128i*>(m_groupIndices.data());
    const __m128i* groupOverlapBits = nullptr;
    if constexpr (Type == GroupingType::overlapAllowed)
    {
        groupOverlapBits =
            reinterpret_cast<const __m128i*>(m_overlapBits.data());
    }
    __m128i complementLO = math::bit_cast<__m128i>(simd::join(r, b));
    __m128i complementHI = math::bit_cast<__m128i>(simd::join(_l, _t));
    __m128i localMaxGroupIndices =
        math::bit_cast<__m128i>(running.maxGroupIndices);
    auto localOverlapBits = math::bit_cast<__m128i>(running.overlapBits);
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
        __m128i partialIsectMasksTB16 =
            _mm_unpackhi_epi8(partialIsectMasks, partialIsectMasks);
        __m128i partialIsectMasksLR16 =
            _mm_unpacklo_epi8(partialIsectMasks, partialIsectMasks);
        // AND LR masks with TB masks for a full LTRB intersection mask.
        __m128i isectMasks16 =
            _mm_and_si128(partialIsectMasksLR16, partialIsectMasksTB16);
        // Mask out the groupIndices that don't intersect.
        __m128i maskedGroupIndices =
            _mm_and_si128(isectMasks16, groupIndices[i]);

        // Doing this as a compile-time check to not have to do the test in the
        // loop at runtime.
        if constexpr (Type == GroupingType::overlapAllowed)
        {
            // Clear any bits from the running overlap where the new index is
            // covering it (i.e. there's a new closest group index)
            // NOTE: that masks and the "and"s are reversed from the non-SSE2
            // code above: SSE2 does not have integral >= or <= operations, so
            // the formulation here uses "!(a > b)" as equivalent to (a <= b),
            // thankfully using no additional instructions thanks to the
            // existence of "andnot" instructions.
            auto clearRunningBits =
                _mm_cmpgt_epi16(maskedGroupIndices, localMaxGroupIndices);

            // andnot is (~a & b) so clearRunningBits needs to come first as
            // it's what we intend to negate
            localOverlapBits =
                _mm_andnot_si128(clearRunningBits, localOverlapBits);

            // Keep any bits from the current group where the running index is
            // not closer (i.e. where current is not covered).
            // Note: this will additionally keep bits where there has not yet
            // been an intersection (index is 0), and the current rectangle was
            // not intersected. This is fine - these bits will get cleared by
            // the above code when a rectangle intersection finally *does*
            // happen, so we can save on an additional mask with isectMask16.
            auto clearCurrentBits =
                _mm_cmpgt_epi16(localMaxGroupIndices, maskedGroupIndices);
            auto maskedOverlapBits =
                _mm_andnot_si128(clearCurrentBits, groupOverlapBits[i]);

            // Combine the two sets, ultimately this will add bits from
            // overlapping rectangles that are in the new-highest draw group and
            // clear any bits that are not.
            localOverlapBits =
                _mm_or_si128(localOverlapBits, maskedOverlapBits);
        }

        // Accumulate max intersecting groupIndices.
        localMaxGroupIndices =
            _mm_max_epi16(maskedGroupIndices, localMaxGroupIndices);
    }
    running.maxGroupIndices = math::bit_cast<int16x8>(localMaxGroupIndices);

    if constexpr (Type == GroupingType::overlapAllowed)
    {
        running.overlapBits = math::bit_cast<uint16x8>(localOverlapBits);
    }

#endif // !FALLBACK_ON_SSE2_INTRINSICS

    // Ensure we never drop below our baseline index.
    if constexpr (Type == GroupingType::overlapAllowed)
    {
        if (running.maxGroupIndices[0] < m_baselineGroupIndex)
        {
            // We're bumping this entry up so the overlap bits need to
            // be cleared.
            running.maxGroupIndices[0] = m_baselineGroupIndex;
            running.overlapBits[0] = m_baselineOverlapBits;
        }
        else if (running.maxGroupIndices[0] == m_baselineGroupIndex)
        {
            running.overlapBits[0] |= m_baselineOverlapBits;
        }
    }
    else
    {
        running.maxGroupIndices[0] =
            std::max(running.maxGroupIndices[0], m_baselineGroupIndex);
    }
    return running;
}

void IntersectionBoard::resizeAndReset(uint32_t viewportWidth,
                                       uint32_t viewportHeight)
{
    m_viewportSize =
        int2{static_cast<int>(viewportWidth), static_cast<int>(viewportHeight)};

    // Divide the board into TILE_DIM x TILE_DIM tiles.
    int2 dims = (m_viewportSize + TILE_DIM - 1) / TILE_DIM;
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
            tileIter->reset(x * TILE_DIM, y * TILE_DIM);
            ++tileIter;
        }
    }
}

int16_t IntersectionBoard::addRectangle(int4 ltrb,
                                        uint16_t currentRectangleOverlapBits,
                                        uint16_t disallowOverlapMask,
                                        int16_t layerCount)
{
    switch (m_groupingType)
    {
        case GroupingType::overlapAllowed:
            return addRectangle<GroupingType::overlapAllowed>(
                ltrb,
                currentRectangleOverlapBits,
                disallowOverlapMask,
                layerCount);
        case GroupingType::disjoint:
            return addRectangle<GroupingType::disjoint>(
                ltrb,
                currentRectangleOverlapBits,
                disallowOverlapMask,
                layerCount);
    }

    RIVE_UNREACHABLE();
}

template <GroupingType Type>
int16_t IntersectionBoard::addRectangle(int4 ltrb,
                                        uint16_t currentRectangleOverlapBits,
                                        uint16_t disallowedOverlapBitsMask,
                                        int16_t layerCount)
{
    if constexpr (Type == GroupingType::overlapAllowed)
    {
        assert(
            layerCount == 1 &&
            "rectangles that are allowed to overlap should only be a single layer");
    }

    // Discard empty, negative, or offscreen rectangles.
    if (simd::any(ltrb.xy >= m_viewportSize || ltrb.zw <= 0 ||
                  ltrb.xy >= ltrb.zw))
    {
        return 0;
    }

    // Clamp ltrb to the viewport to avoid integer overflows.
    ltrb.xy = simd::max(ltrb.xy, int2(0));
    ltrb.zw = simd::min(ltrb.zw, m_viewportSize);

    // Find the tiled row and column that each corner of the rectangle falls on.
    int4 span = (ltrb - int4{0, 0, 1, 1}) / TILE_DIM;
    span = simd::clamp(span, int4(0), int4{m_cols, m_rows, m_cols, m_rows} - 1);
    assert(simd::all(span.xy <= span.zw));

    // Accumulate the max groupIndex from each tile the rectangle touches.
    IntersectionTile::FindResult results;
    for (int y = span.y; y <= span.w; ++y)
    {
        auto tileIter = m_tiles.begin() + y * m_cols + span.x;
        for (int x = span.x; x <= span.z; ++x)
        {
            results =
                tileIter->findMaxIntersectingGroupIndex<Type>(ltrb, results);
            ++tileIter;
        }
    }

    // Find the absolute max group index this rectangle intersects with.
    int16_t bottomGroupIndex = simd::reduce_max(results.maxGroupIndices);
    // It is the caller's responsibility to not insert more rectangles than can
    // fit in a signed 16-bit integer.
    assert(bottomGroupIndex <=
           std::numeric_limits<int16_t>::max() - layerCount);

    if constexpr (Type == GroupingType::overlapAllowed)
    {
        // Clear bits of anything that wasn't in our max draw group, they don't
        // matter (there's already a barrier between them)
        auto overlapMask = (results.maxGroupIndices == bottomGroupIndex);
        results.overlapBits &= overlapMask;

        // Now doing this reduce will give just the bits related to things we
        // overlapped.
        int16_t allOverlapBits = simd::reduce_or(results.overlapBits);
        if ((allOverlapBits & disallowedOverlapBitsMask) != 0)
        {
            // Bits were set in any overlapping rectangle that prevent an
            // overlap here, so we cannot stay in the same group.
            bottomGroupIndex++;
        }
        else
        {
            bottomGroupIndex = std::max(bottomGroupIndex, int16_t(1));
        }
    }
    else
    {
        // With no overlapping allowed, always start in the next group up from
        // the lowest one we intersected (or 1, if no intersection occurred)
        bottomGroupIndex++;
    }

    // Add the rectangle and its newly-found groupIndex to each tile it touches.
    int16_t topGroupIndex = bottomGroupIndex + layerCount - 1;
    for (int y = span.y; y <= span.w; ++y)
    {
        auto tileIter = m_tiles.begin() + y * m_cols + span.x;
        for (int x = span.x; x <= span.z; ++x)
        {
            tileIter->addRectangle<Type>(ltrb,
                                         topGroupIndex,
                                         currentRectangleOverlapBits);
            ++tileIter;
        }
    }

    return bottomGroupIndex;
}

// Explicitly instantiate the public template methods to ensure they get linked

template IntersectionTile::FindResult IntersectionTile::
    findMaxIntersectingGroupIndex<GroupingType::disjoint>(
        int4 ltrb,
        FindResult running) const;
template IntersectionTile::FindResult IntersectionTile::
    findMaxIntersectingGroupIndex<GroupingType::overlapAllowed>(
        int4 ltrb,
        FindResult running) const;

template void IntersectionTile::addRectangle<GroupingType::disjoint>(
    int4 ltrb,
    int16_t groupIndex,
    uint16_t overlapBits);
template void IntersectionTile::addRectangle<GroupingType::overlapAllowed>(
    int4 ltrb,
    int16_t groupIndex,
    uint16_t overlapBits);

#ifdef WITH_RIVE_TOOLS
template void IntersectionTile::testingOnly_addRectangleAndValidate<
    GroupingType::disjoint>(int4 ltrb,
                            int16_t groupIndex,
                            uint16_t overlapBits);
template void IntersectionTile::testingOnly_addRectangleAndValidate<
    GroupingType::overlapAllowed>(int4 ltrb,
                                  int16_t groupIndex,
                                  uint16_t overlapBits);
#endif
} // namespace rive::gpu
