#ifndef _RIVE_RECTANGLES_TO_CONTOUR_HPP_
#define _RIVE_RECTANGLES_TO_CONTOUR_HPP_
#include <vector>
#include <unordered_set>

#include "rive/math/mat2d.hpp"
#include "rive/math/aabb.hpp"

#ifdef TESTING
// When building for testing we use an ordered map for the EdgeMap so we can get
// crossplatform stable order of contours. Visually this doesn't matter but for
// testing across results across different platforms, it does.
#include <map>
struct EdgeMapTesting
{
    bool operator()(const rive::Vec2D& a, const rive::Vec2D& b) const
    {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    }
};
using EdgeMap = std::map<rive::Vec2D, rive::Vec2D, EdgeMapTesting>;
#else
#include <unordered_map>
using EdgeMap = std::unordered_map<rive::Vec2D, rive::Vec2D>;
#endif

namespace rive
{
struct ContourPoint
{
    Vec2D vec;
    int dir; // 0 for horizontal, 1 for vertical

    ContourPoint(const Vec2D& vec, int dir) : vec(vec), dir(dir) {}

    bool operator==(const ContourPoint& other) const
    {
        return vec == other.vec && dir == other.dir;
    }
};

class RectanglesToContour;
class Contour;

// A helper for iterating the points in a contour, lazily skipping points which
// have the same value.
class ContourPointItr
{
public:
    ContourPointItr() = default;
    ContourPointItr(const Span<const ContourPoint> contour, size_t pointIndex) :
        m_contour(contour), m_pointIndex(pointIndex)
    {}

    bool operator!=(const ContourPointItr& that) const
    {
        return m_pointIndex != that.m_pointIndex || m_contour != that.m_contour;
    }
    bool operator==(const ContourPointItr& that) const
    {
        return m_pointIndex == that.m_pointIndex && m_contour == that.m_contour;
    }

    ContourPointItr& operator++();

    Vec2D operator*() const;

private:
    const Span<const ContourPoint> m_contour;
    size_t m_pointIndex;
};

class Contour
{
public:
    Contour(Span<const ContourPoint> points) : m_points(points) {}
    ContourPointItr begin() const { return ContourPointItr(m_points, 0); }
    ContourPointItr end() const
    {
        return ContourPointItr(m_points, m_points.size());
    }

    size_t size() const { return m_points.size(); }
    Vec2D point(size_t index) const { return m_points[index].vec; }
    Vec2D point(size_t index, bool reversed) const
    {
        if (reversed)
        {
            return m_points[m_points.size() - 1 - index].vec;
        }
        return m_points[index].vec;
    }
    bool isClockwise() const;

private:
    Span<const ContourPoint> m_points;
};

// A helper for iterating the contours computed by RectanglesToContour. This
// allows RectanglesToContour to store a linear array of contour points for all
// contours.
class ContourItr
{
public:
    ContourItr() = default;
    ContourItr(const RectanglesToContour* rectanglesToContour,
               size_t contourIndex) :
        m_rectanglesToContour(rectanglesToContour), m_contourIndex(contourIndex)
    {}

    bool operator!=(const ContourItr& that) const
    {
        return m_contourIndex != that.m_contourIndex ||
               m_rectanglesToContour != that.m_rectanglesToContour;
    }
    bool operator==(const ContourItr& that) const
    {
        return m_contourIndex == that.m_contourIndex &&
               m_rectanglesToContour == that.m_rectanglesToContour;
    }

    ContourItr& operator++();

    Contour operator*() const;

private:
    const RectanglesToContour* m_rectanglesToContour;
    size_t m_contourIndex;
};

class RectanglesToContour
{
public:
    // Add a rectangle to the contour computation.
    void addRect(const AABB& rect);
    // Compute the contours once all rects have been added.
    void computeContours();
    // Reset everything to start adding rects again.
    void reset();

    // Results can be queried via the utilities below.
    //
    // For example you can iterate the result:
    //   for (auto contour : converter)
    //   {
    //       for (auto point : contour)
    //       {
    //           printf("contour point: %f %f\n", point.x, point.y);
    //       }
    //   }
    size_t contourCount() const { return m_contourOffsets.size(); }
    Contour contour(size_t index) const;

    ContourItr begin() const { return ContourItr(this, 0); }
    ContourItr end() const { return ContourItr(this, m_contourOffsets.size()); }

    struct RectEvent
    {
        size_t index = 0;
        float size = 0;
        uint8_t type = 0;
        float x = 0;
        float y = 0;

        float getValue(uint8_t axis) const { return axis == 0 ? x : y; }
    };

private:
    // These arrays and maps are built up accumulating temporary data used to
    // build the contours from the rectangles. We store them on the
    // RectanglesToContour class to leverage re-using their reserved memory on
    // re-runs. For this reason it's encouraged to keep the RectanglesToContour
    // around when you know you'll need to recompute the contour from a set of
    // rectangles often/in rapid succession.
    std::vector<RectEvent> m_rectEvents;
    EdgeMap m_edgesH;
    EdgeMap m_edgesV;
    std::unordered_set<Vec2D> m_uniquePoints;
    std::vector<AABB> m_rects;
    std::vector<AABB> m_subdividedRects;
    std::vector<uint8_t> m_rectInclusionBitset;
    std::vector<Vec2D> m_sortedPointsX;
    std::vector<Vec2D> m_sortedPointsY;

    // The entire set of contour points where each contour is tightly packed
    // after the previous at offsets defined in m_contourOffsets.
    std::vector<ContourPoint> m_contourPoints;
    std::vector<size_t> m_contourOffsets;

    bool isRectIncluded(size_t rectIndex);
    void markRectIncluded(size_t rectIndex, bool isIt);

    void addUniquePoint(const Vec2D& point);
    void subdivideRectangles();
};
} // namespace rive
#endif
