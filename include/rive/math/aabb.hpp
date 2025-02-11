#ifndef _RIVE_AABB_HPP_
#define _RIVE_AABB_HPP_

#include "rive/span.hpp"
#include "rive/math/vec2d.hpp"
#include <limits>

namespace rive
{
template <typename T> struct TAABB
{
    int32_t left, top, right, bottom;

    constexpr T width() const { return right - left; }
    constexpr T height() const { return bottom - top; }
    constexpr bool empty() const { return left >= right || top >= bottom; }

    TAABB inset(T dx, T dy) const
    {
        return {left + dx, top + dy, right - dx, bottom - dy};
    }
    TAABB outset(T dx, T dy) const { return inset(-dx, -dy); }
    TAABB offset(T dx, T dy) const
    {
        return {left + dx, top + dy, right + dx, bottom + dy};
    }
    TAABB join(TAABB b) const
    {
        return {std::min(left, b.left),
                std::min(top, b.top),
                std::max(right, b.right),
                std::max(bottom, b.bottom)};
    }
    TAABB intersect(TAABB b) const
    {
        return {std::max(left, b.left),
                std::max(top, b.top),
                std::min(right, b.right),
                std::min(bottom, b.bottom)};
    }

    bool operator==(const TAABB& o) const
    {
        return left == o.left && top == o.top && right == o.right &&
               bottom == o.bottom;
    }
    bool operator!=(const TAABB& o) const { return !(*this == o); }

    bool contains(const TAABB& rhs) const
    {
        return left <= rhs.left && top <= rhs.top && right >= rhs.right &&
               bottom >= rhs.bottom;
    }
};

using IAABB = TAABB<int32_t>;

class AABB
{
public:
    float minX, minY, maxX, maxY;

    AABB() : minX(0), minY(0), maxX(0), maxY(0) {}
    AABB(const Vec2D& min, const Vec2D& max) :
        minX(min.x), minY(min.y), maxX(max.x), maxY(max.y)
    {}
    static AABB fromLTWH(float x, float y, float width, float height)
    {
        return {x, y, x + width, y + height};
    }

    AABB(float minX, float minY, float maxX, float maxY) :
        minX(minX), minY(minY), maxX(maxX), maxY(maxY)
    {}

    AABB(const IAABB& o) :
        AABB((float)o.left, (float)o.top, (float)o.right, (float)o.bottom)
    {}

    AABB(Span<Vec2D>); // computes the union of all points, or 0,0,0,0

    bool operator==(const AABB& o) const
    {
        return minX == o.minX && minY == o.minY && maxX == o.maxX &&
               maxY == o.maxY;
    }
    bool operator!=(const AABB& o) const { return !(*this == o); }

    inline float left() const { return minX; }
    inline float top() const { return minY; }
    inline float right() const { return maxX; }
    inline float bottom() const { return maxY; }

    float width() const { return maxX - minX; }
    float height() const { return maxY - minY; }
    Vec2D size() const { return {width(), height()}; }
    Vec2D center() const
    {
        return {(minX + maxX) * 0.5f, (minY + maxY) * 0.5f};
    }

    bool isEmptyOrNaN() const
    {
        // Use "inverse" logic so we return true if either of the comparisons
        // fail due to a NaN.
        return !(width() > 0 && height() > 0);
    }

    AABB pad(float amount) const { return outset(amount, amount); }

    AABB inset(float dx, float dy) const
    {
        AABB r = {minX + dx, minY + dy, maxX - dx, maxY - dy};
        assert(r.width() >= 0);
        assert(r.height() >= 0);
        return r;
    }
    AABB outset(float dx, float dy) const { return inset(-dx, -dy); }
    AABB offset(float dx, float dy) const
    {
        return {minX + dx, minY + dy, maxX + dx, maxY + dy};
    }

    IAABB round() const;
    IAABB roundOut()
        const; // Rounds out to integer bounds that fully contain the rectangle.

    ///
    /// Initialize an AABB to values that represent an invalid/collapsed
    /// AABB that can then expand to points that are added to it.
    ///
    inline static AABB forExpansion()
    {
        return AABB(std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max());
    }

    ///
    /// Grow the AABB to fit the point.
    ///
    static void expandTo(AABB& out, const Vec2D& point);
    static void expandTo(AABB& out, float x, float y);

    /// Join two AABBs.
    static void join(AABB& out, const AABB& a, const AABB& b);

    void expand(const AABB& other) { join(*this, *this, other); }

    Vec2D factorFrom(Vec2D point) const
    {
        return Vec2D(
            width() == 0.0f ? 0.0f : (point.x - left()) * 2.0f / width() - 1.0f,
            (height() == 0.0f ? 0.0f : point.y - top()) * 2.0f / height() -
                1.0f);
    }

    bool contains(Vec2D position) const;
};

} // namespace rive
#endif
