#pragma once

#include "rive/math/simd.hpp"
#include <vector>
#include "../src/intersection_board.hpp"

namespace rive
{
class IntersectionBoardReferenceImpl
{
public:
    using FindResult = gpu::IntersectionTile::FindResult;

    // For testing IntersectionTile.
    void reset(int left, int top, int16_t baselineGroupIndex)
    {
        m_baselineGroupIndex = baselineGroupIndex;
        m_rects.clear();
        m_groupIndices.clear();
        m_overlapBits.clear();
    }

    // For testing IntersectionBoard.
    void resizeAndReset(uint32_t viewportWidth, uint32_t viewportHeight)
    {
        reset(0, 0, 0);
    }

    // For testing IntersectionTile.
    void addRectangle(int4 ltrb, int16_t groupIndex, uint16_t overlapBits = 0)
    {
        m_rects.push_back(ltrb);
        m_groupIndices.push_back(groupIndex);
        m_overlapBits.push_back(overlapBits);
    }

    // For testing IntersectionTile.
    FindResult findMaxIntersectingGroupIndex(int4 ltrb,
                                             FindResult running) const
    {
        int16_t index = std::max(m_baselineGroupIndex,
                                 simd::reduce_max(running.maxGroupIndices));
        uint16_t overlap = simd::reduce_or(running.overlapBits);

        for (size_t i = 0; i < m_rects.size(); ++i)
        {
            if (simd::all(ltrb.xy < m_rects[i].zw) &&
                simd::all(ltrb.zw > m_rects[i].xy))
            {
                if (m_groupIndices[i] > index)
                {
                    index = m_groupIndices[i];
                    overlap = m_overlapBits[i];
                }
                else if (m_groupIndices[i] == index)
                {
                    overlap |= m_overlapBits[i];
                }
            }
        }

        return {index, overlap};
    }

    template <gpu::GroupingType>
    FindResult findMaxIntersectingGroupIndex(int4 ltrb,
                                             FindResult running) const
    {
        return findMaxIntersectingGroupIndex(ltrb, running);
    }

    // For testing IntersectionBoard.
    int16_t addRectangle(int4 ltrb)
    {
        auto r =
            findMaxIntersectingGroupIndex<gpu::GroupingType::disjoint>(ltrb,
                                                                       {});
        auto groupIndex = r.maxGroupIndices[0] + 1;
        addRectangle(ltrb, groupIndex);
        return groupIndex;
    }

    int16_t addRectangle(int4 ltrb,
                         rive::gpu::GroupingType groupingType,
                         uint16_t overlapBits,
                         uint16_t disallowOverlapMask)
    {
        auto r = findMaxIntersectingGroupIndex(ltrb, {});
        int16_t groupIndex = r.maxGroupIndices[0] + 1;
        if (groupingType == rive::gpu::GroupingType::overlapAllowed &&
            (r.overlapBits[0] & disallowOverlapMask) == 0)
        {
            // Back up into the layer that we can overlap.
            groupIndex = std::max(1, groupIndex - 1);
        }

        addRectangle(ltrb, groupIndex, overlapBits);
        return groupIndex;
    }

private:
    int16_t m_baselineGroupIndex;
    std::vector<int4> m_rects;
    std::vector<int16_t> m_groupIndices;
    std::vector<int16_t> m_overlapBits;
};
} // namespace rive
