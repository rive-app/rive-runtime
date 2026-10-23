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
  RIVE_ALWAYS_INLINE CubicCoeffs(const Vec2D pts[4])
      : CubicCoeffs(
            simd::load2f(&pts[0].x),
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
  RIVE_ALWAYS_INLINE EvalCubic(const Vec2D pts[])
      : EvalCubic(CubicCoeffs(pts), pts[0])
  {}

  RIVE_ALWAYS_INLINE EvalCubic(const CubicCoeffs& coeffs, Vec2D p0)
      : EvalCubic(coeffs, simd::load2f(&p0))
  {}

  RIVE_ALWAYS_INLINE EvalCubic(const CubicCoeffs& coeffs, float2 p0)
      : // Duplicate coefficients across a float4 so we can evaluate two at
        // once.
      A(coeffs.A.xyxy)
      , B((3.f * coeffs.B).xyxy)
      , C((3.f * coeffs.C).xyxy)
      , D(p0.xyxy)
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
float calcPolarSegmentsPerRadian(float approxDevStrokeRadius)
{
  float cosTheta = 1.f - (1.f / Precision) / approxDevStrokeRadius;
  return .5f / acosf(std::max(cosTheta, -1.f));
}

Vec2D evalCubicAt(const Vec2D p[4], float t);

// Given a src cubic bezier, chop it at the specified t value, where 0 <= t <=
// 1, and return the two new cubics in dst: dst[0..3] and dst[3..6]
void chopCubicAt(const Vec2D src[4], Vec2D dst[7], float t);

// Given a src cubic bezier, chop it at the specified t0 and t1 values, where 0
// <= t0 <= t1 <= 1, and return the three new cubics in dst: dst[0..3],
// dst[3..6], and dst[6..9]
void chopCubicAt(const Vec2D src[4], Vec2D dst[10], float t0, float t1);

// Given a src cubic bezier, chop it at the specified t values,
// where 0 <= t0 <= t1 <= ... <= 1, and return the new cubics in dst:
// dst[0..3],dst[3..6],...,dst[3*t_count..3*(t_count+1)]
void chopCubicAt(
    const Vec2D src[4],
    Vec2D dst[],
    const float tValues[],
    int tCount);

// Measures the angle between two vectors, in the range [0, pi].
float measureAngleBetweenVectors(Vec2D a, Vec2D b);

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
int findCubicConvex180Chops(const Vec2D pts[4], float T[2], bool* areCusps);

// This is a cheap test to determine if the full chop-finding code needs to run.
// It runs on four cubics at once and returns a boolean mask. A scalar version
// of this also runs in findCubicConvex180Chops().
int4 cubicsNeedNoChops(
    float4 p0x,
    float4 p0y,
    float4 p1x,
    float4 p1y,
    float4 p2x,
    float4 p2y,
    float4 p3x,
    float4 p3y);

// findCubicConvex180Chops() for four cubics at once, one per lane, except
// for cubics that may have cusps.
struct Convex180Chops4
{
  int4 numChops; // 0, 1 or 2.
  float4 T0;
  float4 T1; // Only valid when numChops == 2.
  // Mask of cubics that may have cusps, which this doesn't handle (cusps are
  // rare in content so they just go through the scalar version instead).
  int4 possibleCusps;
};
Convex180Chops4 findCubicConvex180Chops4(
    float4 p0x,
    float4 p0y,
    float4 p1x,
    float4 p1y,
    float4 p2x,
    float4 p2y,
    float4 p3x,
    float4 p3y);

// Finds the tangents of the curve at T=0 and T=1 respectively.
RIVE_ALWAYS_INLINE Vec2D findCubicTan0(const Vec2D p[4])
{
  Vec2D tan0 = (p[0] != p[1] ? p[1] : p[1] != p[2] ? p[2] : p[3]) - p[0];
  return tan0;
}
RIVE_ALWAYS_INLINE Vec2D findCubicTan1(const Vec2D p[4])
{
  Vec2D tan1 = p[3] - (p[3] != p[2] ? p[2] : p[2] != p[1] ? p[1] : p[0]);
  return tan1;
}
RIVE_ALWAYS_INLINE void findCubicTangents(const Vec2D p[4], Vec2D tangents[2])
{
  tangents[0] = findCubicTan0(p);
  tangents[1] = findCubicTan1(p);
}

// findCubicTangents() for four cubics at once, one per lane. Returns
// {tan0x, tan0y, tan1x, tan1y}.
RIVE_ALWAYS_INLINE simd::gvec4x4<float> findCubicTangents4(
    float4 p0x,
    float4 p0y,
    float4 p1x,
    float4 p1y,
    float4 p2x,
    float4 p2y,
    float4 p3x,
    float4 p3y)
{
  int4 p0p1Differ = (p0x != p1x) | (p0y != p1y);
  int4 p1p2Differ = (p1x != p2x) | (p1y != p2y);
  int4 p2p3Differ = (p2x != p3x) | (p2y != p3y);
  float4 tan0x = simd::if_then_else(
                     p0p1Differ,
                     p1x,
                     simd::if_then_else(p1p2Differ, p2x, p3x)) -
                 p0x;
  float4 tan0y = simd::if_then_else(
                     p0p1Differ,
                     p1y,
                     simd::if_then_else(p1p2Differ, p2y, p3y)) -
                 p0y;
  float4 tan1x = p3x - simd::if_then_else(
                           p2p3Differ,
                           p2x,
                           simd::if_then_else(p1p2Differ, p1x, p0x));
  float4 tan1y = p3y - simd::if_then_else(
                           p2p3Differ,
                           p2y,
                           simd::if_then_else(p1p2Differ, p1y, p0y));
  return {tan0x, tan0y, tan1x, tan1y};
}

RIVE_ALWAYS_INLINE constexpr float pow2(float x) { return x * x; }
RIVE_ALWAYS_INLINE constexpr float pow3(float x) { return x * pow2(x); }
RIVE_ALWAYS_INLINE constexpr float lengthPow2(Vec2D v)
{
  return pow2(v.x) + pow2(v.y);
}
} // namespace math
} // namespace rive
