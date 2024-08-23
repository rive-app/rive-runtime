#include "rive/math/math_types.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/simd.hpp"
#include <algorithm>
#include <cmath>

using namespace rive;

AABB::AABB(Span<Vec2D> pts)
{
    if (pts.size() == 0)
    {
        minX = minY = maxX = maxY = 0;
        return;
    }

    float L = pts[0].x, R = L, T = pts[0].y, B = T;

    for (size_t i = 1; i < pts.size(); ++i)
    {
        L = std::min(L, pts[i].x);
        R = std::max(R, pts[i].x);
        T = std::min(T, pts[i].y);
        B = std::max(B, pts[i].y);
    }
    minX = L;
    maxX = R;
    minY = T;
    maxY = B;
}

static inline float graphics_roundf(float x) { return std::floor(x + 0.5f); }

static inline int graphics_round(float x) { return (int)graphics_roundf(x); }

IAABB AABB::round() const
{
    return {
        graphics_round(left()),
        graphics_round(top()),
        graphics_round(right()),
        graphics_round(bottom()),
    };
}

IAABB AABB::roundOut() const
{
    float4 bounds = simd::load4f(this);
    bounds.xy = simd::floor(bounds.xy);
    bounds.zw = simd::ceil(bounds.zw);
    return math::bit_cast<IAABB>(simd::cast<int32_t>(bounds));
}

void AABB::expandTo(AABB& out, const Vec2D& point) { expandTo(out, point.x, point.y); }

void AABB::expandTo(AABB& out, float x, float y)
{
    if (x < out.minX)
    {
        out.minX = x;
    }
    if (x > out.maxX)
    {
        out.maxX = x;
    }
    if (y < out.minY)
    {
        out.minY = y;
    }
    if (y > out.maxY)
    {
        out.maxY = y;
    }
}

void AABB::join(AABB& out, const AABB& a, const AABB& b)
{
    out.minX = std::min(a.minX, b.minX);
    out.minY = std::min(a.minY, b.minY);
    out.maxX = std::max(a.maxX, b.maxX);
    out.maxY = std::max(a.maxY, b.maxY);
}

bool AABB::contains(Vec2D point) const
{
    return point.x >= left() && point.x <= right() && point.y >= top() && point.y <= bottom();
}
