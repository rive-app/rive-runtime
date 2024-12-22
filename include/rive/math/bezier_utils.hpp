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

// Optimized SIMD helper for evaluating a single cubic at many points.
class EvalCubic
{
public:
    inline EvalCubic(const Vec2D pts[])
    {
        // Cubic power-basis form:
        //
        //                                       | 1  0  0  0|   |P0|
        //   Cubic(T) = x,y = |1  t  t^2  t^3| * |-3  3  0  0| * |P1|
        //                                       | 3 -6  3  0|   |P2|
        //                                       |-1  3 -3  1|   |P3|
        //
        // Find the cubic's power basis coefficients. These define the bezier
        // curve as:
        //
        //                                  |t^3|
        //     Cubic(T) = x,y = |A  B  C| * |t^2| + P0
        //                      |.  .  .|   |t  |
        //
        // (Duplicate coefficients across a float4 so we can evaluate two at
        // once.)
        m_P0 = simd::load2f(pts + 0).xyxy;
        float4 P1 = simd::load2f(pts + 1).xyxy;
        float4 P2 = simd::load2f(pts + 2).xyxy;
        float4 P3 = simd::load2f(pts + 3).xyxy;
        m_C = 3.f * (P1 - m_P0);
        float4 D = 3.f * (P2 - P1);
        float4 E = P3 - m_P0;
        m_B = D - m_C;
        m_A = E - D;
    }

    // Evaluates [x, y] at location t.
    inline float2 operator()(float t) const
    {
        // At^3 + Bt^2 + Ct + P0
        return t * (t * (t * m_A.xy + m_B.xy) + m_C.xy) + m_P0.xy;
    }

    // Evaluates [Xa, Ya, Xb, Yb] at locations [Ta, Ta, Tb, Tb].
    inline float4 operator()(float4 t) const
    {
        // At^3 + Bt^2 + Ct + P0
        return t * (t * (t * m_A + m_B) + m_C) + m_P0;
    }

private:
    float4 m_A;
    float4 m_B;
    float4 m_C;
    float4 m_P0;
};
} // namespace math
} // namespace rive
