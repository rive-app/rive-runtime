/*
 * Copyright 2024 Rive
 */

#pragma once

#include "rive/math/simd.hpp"
#include "rive/enum_bitset.hpp"
#include <vector>

namespace rive::gpu
{

// Adding rectangles allows a grouping type to be specified - this determines
// whether a rectangle is allowed to share a group with another that it
// overlaps.
enum class GroupingType : bool
{
    // The current rectangle cannot be grouped with any others for any reason,
    // so no additional testing/logic is necessary. (this is the default for
    // most render modes)
    disjoint,

    // The current rectangle is potentially allowed to group with others it
    // overlaps, assuming the test of "overlap bits" succeeds.
    overlapAllowed,
};

// Tile within an intersection board that manages a set of rectangles and their
// groupIndex. From a given rectangle, finds the max groupIndex in the set of
// internal rectangles it intersects and, when needed, also finds the full set
// of overlap bits from any interesections. The size is <= 255 so we can store
// bounding box coordinates in 8 bits.
class IntersectionTile
{
public:
    // The width and height of an intersection tile (must fit in an unsigned
    // byte).
    constexpr static int TILE_DIM = 255;
    static_assert(TILE_DIM <= std::numeric_limits<uint8_t>::max());

    // Performance is better passing these two vectors as a struct than using
    // in/out parameters.
    struct FindResult
    {
        int16x8 maxGroupIndices = 0;
        uint16x8 overlapBits = 0;
    };

    void reset(int left,
               int top,
               int16_t baselineGroupIndex = 0,
               uint16_t baselineOverlapBits = 0);

    template <GroupingType Type>
    void addRectangle(int4 ltrb,
                      int16_t groupIndex,
                      uint16_t currentRectangleOverlapBits);

#ifdef WITH_RIVE_TOOLS
    // a testing-specific version that validates after doing the add
    template <GroupingType Type>
    void testingOnly_addRectangleAndValidate(
        int4 ltrb,
        int16_t groupIndex,
        uint16_t currentRectangleOverlapBits);
#endif

    // Accumulate local maximum intersecting group indices for the given
    // rectangle in each channel of a int16x8. "runningMaxGroupIndices" is a
    // running set of local maximums if the IntersectionBoard also ran this same
    // test on other tile(s) that the rectangle touched. The absolute maximum
    // group index that this rectangle intersects with will be
    // simd::reduce_max(result.maxGroupIndices), and the relevant overlap bits
    // will be simd::reduce_or(result.overlapBits)
    template <GroupingType Type>
    FindResult findMaxIntersectingGroupIndex(int4 ltrb,
                                             FindResult runningResult) const;

    // The following were exposed for unit testing

#ifdef WITH_RIVE_TOOLS
    size_t testingOnly_rectangleCount() const { return m_rectangleCount; }
    int16_t testingOnly_baselineGroupIndex() const
    {
        return m_baselineGroupIndex;
    }
    uint16_t testingOnly_baselineOverlapBits() const
    {
        return m_baselineOverlapBits;
    }

    int16_t testingOnly_maxGroupIndex() const { return m_maxGroupIndex; }
    uint16_t testingOnly_overlapBitsForMaxGroup() const
    {
        return m_overlapBitsForMaxGroup;
    }
#endif

private:
    // Set the baseline to equal the current max group index
    template <GroupingType Type>
    void updateBaselineToMaxGroupIndex(uint16_t additionalBaselineOverlapBits);

    int4 m_topLeft = {};
    int16_t m_baselineGroupIndex = 0;
    uint16_t m_baselineOverlapBits = 0;
    int16_t m_maxGroupIndex = 0;
    uint16_t m_overlapBitsForMaxGroup = 0;
    size_t m_rectangleCount = 0;

    // We may need a bias to get the tile coordinates to fit within a signed
    // 8-bit value (because base-level SSE does not have unsigned comparisons)
    constexpr static int TILE_EDGE_BIAS =
        (TILE_DIM > std::numeric_limits<int8_t>::max()) ? -128 : 0;

    // How many rectangles/groupIndices are in each chunk of data?
    constexpr static size_t CHUNK_SIZE = 8;

    // Chunk of 8 rectangles encoded as [L, T, TILE_DIM - R, TILE_DIM - B],
    // relative to m_left and m_top. The data is also transposed:
    // [L0..L7, T0..T7, -R0..R7, -B0..B7].
    std::vector<int8x32> m_edges;
    static_assert(sizeof(m_edges[0]) == CHUNK_SIZE * 4);

    // Chunk of 8 groupIndices corresponding to the above edges.
    std::vector<int16x8> m_groupIndices;
    static_assert(sizeof(m_groupIndices[0]) == CHUNK_SIZE * 2);

    // Chunk of 8 sets of overlap bits corresponding to the above edges.
    std::vector<uint16x8> m_overlapBits;
    static_assert(sizeof(m_overlapBits[0]) == CHUNK_SIZE * 2);
};

// Manages a set of rectangles and their groupIndex across a variable-sized
// viewport. Each time a rectangle is added, assigns and returns a groupIndex
// that is one larger than the max groupIndex in the set of existing rectangles
// it intersects.
class IntersectionBoard
{
public:
    constexpr static auto TILE_DIM = IntersectionTile::TILE_DIM;

    IntersectionBoard(GroupingType groupingType) : m_groupingType(groupingType)
    {}

    void resizeAndReset(uint32_t viewportWidth, uint32_t viewportHeight);

    // Adds a rectangle to the internal set and assigns it "layerCount"
    // contiguous group indices, beginning one larger than the max groupIndex in
    // the set of existing rectangles it intersects.
    //
    // Returns the first newly assigned groupIndex for the added rectangle.
    // If it does not intersect with any other rectangles, this groupIndex is 1.
    //
    // It is the caller's responsibility to not insert more rectangles than can
    // fit in a signed 16-bit integer. (The result is signed because SSE doesn't
    // have an unsigned max instruction.)
    int16_t addRectangle(
        int4 ltrb,
        uint16_t
            currentRectangleOverlapBits, // "what is allowed to overlap me?"
        uint16_t
            disallowedOverlapBitsMask, // "what am I not allowed to overlap?"
        int16_t layerCount);

    int16_t addRectangle(int4 ltrb, int16_t layerCount = 1)
    {
        return addRectangle(ltrb, 0, 0, layerCount);
    }

    GroupingType groupingType() const { return m_groupingType; }

private:
    template <GroupingType Type>
    int16_t addRectangle(int4 ltrb,
                         uint16_t currentRectangleOverlapBits,
                         uint16_t disallowedOverlapBitsMask,
                         int16_t layerCount);

    GroupingType m_groupingType{};
    int2 m_viewportSize{};
    int32_t m_cols = 0;
    int32_t m_rows = 0;
    std::vector<IntersectionTile> m_tiles;
};
} // namespace rive::gpu
