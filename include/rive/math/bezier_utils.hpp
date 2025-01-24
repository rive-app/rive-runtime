/*
 * Copyright 2021 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Initial import from skia:src/gpu/tessellate/Tessellation.h
 *                     skia:src/core/SkGeometry.h
 *
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/simd.hpp"
#include "rive/math/vec2d.hpp"
#include <math.h>

namespace rive
{
namespace math
{
// Finds the cubic bezier's power basis coefficients. These define the bezier
// curve as:
//
//                                    |T^3|
//     Cubic(T) = x,y = |A  3B  3C| * |T^2| + P0
//                      |.   .   .|   |T  |
//
// And the tangent:
//
//                                         |T^2|
//     Tangent(T) = dx,dy = |3A  6B  3C| * |T  |
//                          | .   .   .|   |1  |
//
struct CubicCoeffs
{
    RIVE_ALWAYS_INLINE CubicCoeffs(const Vec2D pts[4]) :
        CubicCoeffs(simd::load2f(&pts[0].x),
                    simd::load2f(&pts[1].x),
                    simd::load2f(&pts[2].x),
                    simd::load2f(&pts[3].x))
    {}

    RIVE_ALWAYS_INLINE CubicCoeffs(float2 p0, float2 p1, float2 p2, float2 p3)
    {
        C = p1 - p0;
        float2 D = p2 - p1;
        float2 E = p3 - p0;
        B = D - C;
        A = -3.f * D + E;
    }

    float2 A, B, C;
};

// Optimized SIMD helper for evaluating a single cubic at many points.
class EvalCubic
{
public:
    RIVE_ALWAYS_INLINE EvalCubic(const Vec2D pts[]) :
        EvalCubic(CubicCoeffs(pts), pts[0])
    {}

    RIVE_ALWAYS_INLINE EvalCubic(const CubicCoeffs& coeffs, Vec2D p0) :
        EvalCubic(coeffs, simd::load2f(&p0))
    {}

    RIVE_ALWAYS_INLINE EvalCubic(const CubicCoeffs& coeffs, float2 p0) :
        // Duplicate coefficients across a float4 so we can evaluate two at
        // once.
        A(coeffs.A.xyxy),
        B((3.f * coeffs.B).xyxy),
        C((3.f * coeffs.C).xyxy),
        D(p0.xyxy)
    {}

    // Evaluates [x, y] at location t.
    RIVE_ALWAYS_INLINE float2 operator()(float t) const
    {
        // At^3 + Bt^2 + Ct + P0
        return ((A.xy * t + B.xy) * t + C.xy) * t + D.xy;
    }

    // Evaluates [Xa, Ya, Xb, Yb] at locations [Ta, Ta, Tb, Tb].
    RIVE_ALWAYS_INLINE float4 operator()(float4 t) const
    {
        // At^3 + Bt^2 + Ct + P0
        return ((A * t + B) * t + C) * t + D;
    }

    RIVE_ALWAYS_INLINE float4 at(float4 t) const
    {
        // At^3 + Bt^2 + Ct + P0
        return ((A * t + B) * t + C) * t + D;
    }

private:
    const float4 A;
    const float4 B;
    const float4 C;
    const float4 D;
};

// Decides the number of polar segments the tessellator adds for each curve.
// (Uniform steps in tangent angle.) The tessellator will add this number of
// polar segments for each radian of rotation in local path space.
template <int Precision>
float calc_polar_segments_per_radian(float approxDevStrokeRadius)
{
    float cosTheta = 1.f - (1.f / Precision) / approxDevStrokeRadius;
    return .5f / acosf(std::max(cosTheta, -1.f));
}

Vec2D eval_cubic_at(const Vec2D p[4], float t);

// Given a src cubic bezier, chop it at the specified t value,
// where 0 <= t <= 1, and return the two new cubics in dst:
// dst[0..3] and dst[3..6]
void chop_cubic_at(const Vec2D src[4], Vec2D dst[7], float t);

// Given a src cubic bezier, chop it at the specified t0 and t1 values,
// where 0 <= t0 <= t1 <= 1, and return the three new cubics in dst:
// dst[0..3], dst[3..6], and dst[6..9]
void chop_cubic_at(const Vec2D src[4], Vec2D dst[10], float t0, float t1);

// Given a src cubic bezier, chop it at the specified t values,
// where 0 <= t0 <= t1 <= ... <= 1, and return the new cubics in dst:
// dst[0..3],dst[3..6],...,dst[3*t_count..3*(t_count+1)]
void chop_cubic_at(const Vec2D src[4],
                   Vec2D dst[],
                   const float tValues[],
                   int tCount);

// Measures the angle between two vectors, in the range [0, pi].
float measure_angle_between_vectors(Vec2D a, Vec2D b);

// Returns 0, 1, or 2 T values at which to chop the given curve in order to
// guarantee the resulting cubics are convex and rotate no more than 180
// degrees.
//
//   - If the cubic is "serpentine", then the T values are any inflection points
//   in [0 < T < 1].
//   - If the cubic is linear, then the T values are any 180-degree cusp points
//   in [0 < T < 1].
//   - Otherwise the T value is the point at which rotation reaches 180 degrees,
//   iff in [0 < T < 1].
//
// 'areCusps' is set to true if the chop point occurred at a cusp (within
// tolerance), or if the chop point(s) occurred at 180-degree turnaround points
// on a degenerate flat line.
int find_cubic_convex_180_chops(const Vec2D[], float T[2], bool* areCusps);

// Returns up to 4 T values at which to chop the given curve in order to
// guarantee the resulting cubics are convex and rotate no more than 90 degrees.
//
// If the curve has any cusp points (proper cusps or 180-degree turnarounds on
// a degenerate flat line), the cusps are straddled with `cuspPadding` on either
// side and `areCusps` is set to true. In this cases, odd-numbered curves after
// chopping will always be the small sections that pass through the cusp.
int find_cubic_convex_90_chops(const Vec2D[],
                               float T[4],
                               float cuspPadding,
                               bool* areCusps);

// Find the location and value of a cubic's maximum height, relative to the
// baseline p0->p3.
float find_cubic_max_height(const Vec2D pts[4], float* outT);

// Measure the amount of curvature, in radians, of the given cubic, centered at
// location T, and covering a spread of width "desiredSpread" in local
// coordinates. If "desiredSpread" would reach outside the range t=0..1, a
// smaller spread is used.
float measure_cubic_local_curvature(const Vec2D pts[4],
                                    const math::CubicCoeffs& coeffs,
                                    float T,
                                    float desiredSpread);
inline float measure_cubic_local_curvature(const Vec2D pts[4],
                                           float T,
                                           float desiredSpread)
{
    return measure_cubic_local_curvature(pts,
                                         CubicCoeffs(pts),
                                         T,
                                         desiredSpread);
}
} // namespace math
} // namespace rive
