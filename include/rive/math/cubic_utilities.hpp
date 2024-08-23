#ifndef _RIVE_CUBIC_UTILITIES_HPP_
#define _RIVE_CUBIC_UTILITIES_HPP_

#include "rive/math/vec2d.hpp"
#include <algorithm>

namespace rive
{
///
/// Utility functions for recursively subdividing a cubic.
///
class CubicUtilities
{
public:
    static void computeHull(const Vec2D& from,
                            const Vec2D& fromOut,
                            const Vec2D& toIn,
                            const Vec2D& to,
                            float t,
                            Vec2D* hull)
    {
        hull[0] = Vec2D::lerp(from, fromOut, t);
        hull[1] = Vec2D::lerp(fromOut, toIn, t);
        hull[2] = Vec2D::lerp(toIn, to, t);

        hull[3] = Vec2D::lerp(hull[0], hull[1], t);
        hull[4] = Vec2D::lerp(hull[1], hull[2], t);

        hull[5] = Vec2D::lerp(hull[3], hull[4], t);
    }

    static bool tooFar(const Vec2D& a, const Vec2D& b, float threshold)
    {
        return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)) > threshold;
    }

    static bool shouldSplitCubic(const Vec2D& from,
                                 const Vec2D& fromOut,
                                 const Vec2D& toIn,
                                 const Vec2D& to,
                                 float threshold)
    {

        Vec2D oneThird = Vec2D::lerp(from, to, 1.0f / 3.0f);
        Vec2D twoThird = Vec2D::lerp(from, to, 2.0f / 3.0f);
        return tooFar(fromOut, oneThird, threshold) || tooFar(toIn, twoThird, threshold);
    }

    static float cubicAt(float t, float a, float b, float c, float d)
    {
        float ti = 1.0f - t;
        float value =
            ti * ti * ti * a + 3.0f * ti * ti * t * b + 3.0f * ti * t * t * c + t * t * t * d;
        return value;
    }
};
} // namespace rive
#endif