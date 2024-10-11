#pragma once

#include "rive/math/simd.hpp"
#include <vector>

namespace rive
{
class IntersectionBoardReferenceImpl
{
public:
    // For testing IntersectionTile.
    void reset(int left, int top, int16_t baselineGroupIndex)
    {
        m_baselineGroupIndex = baselineGroupIndex;
        m_rects.clear();
        m_groupIndices.clear();
    }

    // For testing IntersectionBoard.
    void resizeAndReset(uint32_t viewportWidth, uint32_t viewportHeight)
    {
        reset(0, 0, 0);
    }

    // For testing IntersectionTile.
    void addRectangle(int4 ltrb, int16_t groupIndex)
    {
        m_rects.push_back(ltrb);
        m_groupIndices.push_back(groupIndex);
    }

    // For testing IntersectionTile.
    int16x8 findMaxIntersectingGroupIndex(
        int4 ltrb,
        int16x8 baselineGroupIndices = 0) const
    {
        int16_t index = m_baselineGroupIndex;
        for (size_t i = 0; i < m_rects.size(); ++i)
        {
            if (simd::all(ltrb.xy < m_rects[i].zw) &&
                simd::all(ltrb.zw > m_rects[i].xy))
            {
                index = std::max(index, m_groupIndices[i]);
            }
        }
        return index;
    }

    // For testing IntersectionBoard.
    int16_t addRectangle(int4 ltrb)
    {
        int16_t groupIndex = findMaxIntersectingGroupIndex(ltrb)[0] + 1;
        addRectangle(ltrb, groupIndex);
        return groupIndex;
    }

private:
    int16_t m_baselineGroupIndex;
    std::vector<int4> m_rects;
    std::vector<int16_t> m_groupIndices;
};
} // namespace rive
