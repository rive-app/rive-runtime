#ifndef _RIVE_RECTANGLES_TO_CONTOUR_HPP_
#define _RIVE_RECTANGLES_TO_CONTOUR_HPP_
#include <vector>
#include <unordered_set>
#include "rive/math/mat2d.hpp"
#include "rive/math/rect.hpp"

namespace rive
{
struct PolygonPoint
{
    Vec2D vec;
    int dir; // 0 for horizontal, 1 for vertical

    PolygonPoint(const Vec2D& vec, int dir) : vec(vec), dir(dir) {}

    bool operator==(const PolygonPoint& other) const
    {
        return vec == other.vec && dir == other.dir;
    }
};

struct RectanglesToContour
{
private:
    std::unordered_set<Vec2D> uniquePoints;
    std::vector<Rect> rects;
    void addUniquePoint(const Vec2D& point);
    void addRect(const Rect& rect);
    std::vector<std::vector<Vec2D>> computeContours();

public:
    static std::vector<std::vector<Vec2D>> makeSelectionContours(
        const std::vector<Rect>& rects);
};
} // namespace rive
#endif
