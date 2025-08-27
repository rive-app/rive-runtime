#ifndef _RIVE_SEGMENTED_CONTOUR_HPP_
#define _RIVE_SEGMENTED_CONTOUR_HPP_

#include "rive/math/vec2d.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/aabb.hpp"
#include <vector>

namespace rive
{
class RawPath;

/// Utilty for converting a RawPath into a contour segments.
class SegmentedContour
{
private:
    std::vector<Vec2D> m_contourPoints;

    AABB m_bounds;
    float m_threshold;
    float m_thresholdSquared;

    void addVertex(Vec2D vertex);
    void segmentCubic(const Vec2D& from,
                      const Vec2D& fromOut,
                      const Vec2D& toIn,
                      const Vec2D& to,
                      float t1,
                      float t2);

public:
    const Span<const Vec2D> contourPoints(uint32_t endOffset = 0) const;
    const std::size_t contourSize() const;

    SegmentedContour(float threshold);

    float threshold() const;
    void threshold(float value);
    const AABB& bounds() const;

    void contour(const RawPath& rawPath, const Mat2D& transform);
};
} // namespace rive
#endif
