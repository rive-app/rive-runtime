#ifndef _RIVE_PATH_MEASURE_HPP_
#define _RIVE_PATH_MEASURE_HPP_

#include "rive/math/contour_measure.hpp"
#include <vector>

namespace rive
{
class PathMeasure
{
public:
    PathMeasure();
    PathMeasure(const RawPath* path,
                float tol = ContourMeasureIter::kDefaultTolerance);
    ContourMeasure::PosTanDistance atDistance(float distance) const;
    ContourMeasure::PosTanDistance atPercentage(float percentageDistance) const;

    void getSegment(float startDistance,
                    float endDistance,
                    RawPath* dst,
                    bool startWithMove = true) const;

    float length() const { return m_length; }
    bool isClosed() const;

private:
    float m_length;
    std::vector<rcp<ContourMeasure>> m_contours;
};
} // namespace rive

#endif
