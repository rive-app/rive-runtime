/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Initial import from skia:src/core/SkGeometry.cpp
 *                     skia:src/gpu/tessellate/Tessellation.cpp
 *
 * Copyright 2022 Rive
 */

#include "rive/math/bezier_utils.hpp"

#include "rive/math/math_types.hpp"

namespace rive
{
namespace math
{
Vec2D evalCubicAt(const Vec2D p[4], float t)
{
  float2 p0 = simd::load2f(p + 0);
  float2 p1 = simd::load2f(p + 1);
  float2 p2 = simd::load2f(p + 2);
  float2 p3 = simd::load2f(p + 3);
  float2 a = p3 + 3.f * (p1 - p2) - p0;
  float2 b = 3.f * (p2 - 2.f * p1 + p0);
  float2 c = 3.f * (p1 - p0);
  float2 d = p0;
  return math::bit_cast<Vec2D>(((a * t + b) * t + c) * t + d);
}

void chopCubicAt(const Vec2D src[4], Vec2D dst[7], float t)
{
  assert(0 <= t && t <= 1);

  if (t == 1)
  {
    memcpy(dst, src, sizeof(Vec2D) * 4);
    dst[4] = dst[5] = dst[6] = src[3];
    return;
  }

  float4 p0p1 = simd::load4f(src);
  float4 p1p2 = simd::load4f(src + 1);
  float4 p2p3 = simd::load4f(src + 2);
  float4 T = t;

  float4 ab_bc = simd::mix(p0p1, p1p2, T);
  float4 bc_cd = simd::mix(p1p2, p2p3, T);
  float4 abc_bcd = simd::mix(ab_bc, bc_cd, T);
  float2 abcd = simd::mix(abc_bcd.xy, abc_bcd.zw, T.xy);

  simd::store(dst + 0, p0p1.xy);
  simd::store(dst + 1, ab_bc.xy);
  simd::store(dst + 2, abc_bcd.xy);
  simd::store(dst + 3, abcd);
  simd::store(dst + 4, abc_bcd.zw);
  simd::store(dst + 5, bc_cd.zw);
  simd::store(dst + 6, p2p3.zw);
}

void chopCubicAt(const Vec2D src[4], Vec2D dst[10], float t0, float t1)
{
  assert(0 <= t0 && t0 <= t1 && t1 <= 1);

  if (t1 == 1)
  {
    chopCubicAt(src, dst, t0);
    dst[7] = dst[8] = dst[9] = src[3];
    return;
  }

  // Perform both chops in parallel using 4-lane SIMD.
  float4 p00, p11, p22, p33, T;
  p00 = simd::load2f(src + 0).xyxy;
  p11 = simd::load2f(src + 1).xyxy;
  p22 = simd::load2f(src + 2).xyxy;
  p33 = simd::load2f(src + 3).xyxy;
  T.xy = t0;
  T.zw = t1;

  float4 ab = simd::mix(p00, p11, T);
  float4 bc = simd::mix(p11, p22, T);
  float4 cd = simd::mix(p22, p33, T);
  float4 abc = simd::mix(ab, bc, T);
  float4 bcd = simd::mix(bc, cd, T);
  float4 abcd = simd::mix(abc, bcd, T);
  float4 middle = simd::mix(abc, bcd, T.zwxy);

  simd::store(dst + 0, p00.xy);
  simd::store(dst + 1, ab.xy);
  simd::store(dst + 2, abc.xy);
  simd::store(dst + 3, abcd.xy);
  simd::store(dst + 4, middle); // 2 points!
  // dst + 5 written above.
  simd::store(dst + 6, abcd.zw);
  simd::store(dst + 7, bcd.zw);
  simd::store(dst + 8, cd.zw);
  simd::store(dst + 9, p33.zw);
}

void chopCubicAt(
    const Vec2D src[4],
    Vec2D dst[],
    const float tValues[],
    int tCount)
{
  assert(dst);
  assert(
      tValues == nullptr || std::all_of(tValues, tValues + tCount, [](float t) {
        return t >= 0 && t <= 1;
      }));
  assert(tValues == nullptr || std::is_sorted(tValues, tValues + tCount));

  if (tCount == 0)
  {
    // nothing to chop
    memcpy(dst, src, 4 * sizeof(Vec2D));
  }
  else
  {
    int i = 0;
    float lastT = 0;
    for (; i < tCount - 1; i += 2)
    {
      // Do two chops at once.
      float2 tt;
      if (tValues != nullptr)
      {
        tt = simd::load2f(tValues + i);
        tt = simd::clamp((tt - lastT) / (1 - lastT), float2(0), float2(1));
        lastT = tValues[i + 1];
      }
      else
      {
        tt = float2{1, 2} / static_cast<float>(tCount + 1 - i);
      }
      chopCubicAt(src, dst, tt[0], tt[1]);
      src = dst = dst + 6;
    }
    if (i < tCount)
    {
      // Chop the final cubic if there was an odd number of chops.
      assert(i + 1 == tCount);
      float t = tValues != nullptr ? tValues[i] : .5f;
      t = simd::clamp<float, 1>(
              math::ieee_float_divide(t - lastT, 1 - lastT),
              0,
              1)
              .x;
      chopCubicAt(src, dst, t);
    }
  }
}

float measureAngleBetweenVectors(Vec2D a, Vec2D b)
{
  float cosTheta = math::ieee_float_divide(
      Vec2D::dot(a, b),
      sqrtf(Vec2D::dot(a, a) * Vec2D::dot(b, b)));
  // Pin cosTheta such that if it is NaN (e.g., if a or b was 0), then we
  // return acos(1) = 0.
  cosTheta = std::max(std::min(1.f, cosTheta), -1.f);
  return acosf(cosTheta);
}

// If a chop falls within a distance of "TESS_EPSILON" from 0 or 1, throw it
// out. Tangents become unstable when we chop too close to the boundary. This
// works out because the tessellation shaders don't allow more than 2^10
// parametric segments, and they snap the beginning and ending edges at 0 and 1.
// So if we overstep an inflection or point of 180-degree rotation by a fraction
// of a tessellation segment, it just gets snapped.
constexpr static float TESS_EPSILON = 1.f / (1 << 10);

// Floating-point representation of "1 - 2*TESS_EPSILON".
constexpr static uint32_t IEEEOneMinus2Epsilon = (127 << 23) -
                                                 2 * (1 << (24 - 10));
#ifdef RIVE_HAS_CONSTEXPR_BIT_CAST
static_assert(
    math::bit_cast<float>(IEEEOneMinus2Epsilon) == 1 - 2 * TESS_EPSILON);
#endif

int findCubicConvex180Chops(const Vec2D pts[4], float T[2], bool* areCusps)
{
  assert(pts);
  assert(T);
  assert(areCusps);

#ifndef RIVE_HAS_CONSTEXPR_BIT_CAST
  // Without a constexpr bit_cast, check the bits at runtime instead.
  assert(math::bit_cast<float>(IEEEOneMinus2Epsilon) == 1 - 2 * TESS_EPSILON);
#endif

  float2 p0 = simd::load2f(&pts[0].x);
  float2 p1 = simd::load2f(&pts[1].x);
  float2 p2 = simd::load2f(&pts[2].x);
  float2 p3 = simd::load2f(&pts[3].x);

  // Build the three edges between the four control points. We use this for
  // some early-out tests below.
  float2 e01 = p1 - p0;
  float2 e12 = p2 - p1;
  float2 e23 = p3 - p2;

  // The sign of the cross product gives us the winding orientation of three
  // points
  float c01x12 = simd::cross(e01, e12);
  float c12x23 = simd::cross(e12, e23);
  float c01x23 = simd::cross(e01, e23);

  bool e01IsZero = simd::all(e01 == 0.0f);
  bool e23IsZero = simd::all(e23 == 0.0f);

  // If the first edge is zero, we have a triangle, and there's only one
  // relevant turn (p1-p2-p3). Duplicate it to simplify the convexity test
  // below.
  if (e01IsZero)
  {
    c01x12 = c12x23;
    c01x23 = c12x23;
  }

  // Same logic, but for the last edge. Note that if both the first and last
  // edges are zero, all of the cAAxBB values will be zero, causing isConvex
  // to become false. In this case, isLine will detect an actual line versus
  // the degenerate single point case.
  if (e23IsZero)
  {
    c12x23 = c01x12;
    c01x23 = c01x12;
  }

  float turnMin = std::min(std::min(c01x12, c12x23), c01x23);
  float turnMax = std::max(std::max(c01x12, c12x23), c01x23);

  // Cross products and their rounding error both scale with the squared edge
  // lengths, so the threshold scales with them too.
  float maxEdgeLengthSquared = std::max(
      std::max(simd::dot(e01, e01), simd::dot(e12, e12)),
      simd::dot(e23, e23));
  float turnThreshold = EPSILON * maxEdgeLengthSquared;

  // If the windings of both edge pairs are in the same direction, there's no
  // inflection point. Furthermore, if (p0-p1) x (p2-p3) also winds in that
  // same direction, then we haven't exceeded 180 degrees.
  bool isConvex = turnMin > turnThreshold || turnMax < -turnThreshold;
  bool isLine = e01IsZero && e23IsZero && simd::any(e12 != 0.0f);
  if (isConvex || isLine)
  {
    *areCusps = false;
    return 0;
  }

  CubicCoeffs coeffs(p0, p1, p2, p3);

  // Now find the cubic's inflection function.
  // There are inflections where F' x F'' == 0.
  //
  // We formulate this as a quadratic equation:
  //
  //     F' x F'' == aT^2 + bT + c == 0.
  //
  // See:
  // https://www.microsoft.com/en-us/research/wp-content/uploads/2005/01/p1000-loop.pdf
  // NOTE: We only need the roots, so a uniform scale factor does not affect
  // the solution.
  float a = simd::cross(coeffs.A, coeffs.B);
  float b = simd::cross(coeffs.A, coeffs.C);
  float c = simd::cross(coeffs.B, coeffs.C);
  float b_over_minus_2 = -.5f * b;
  float discr_over_4 = b_over_minus_2 * b_over_minus_2 - a * c;

  // If -cuspThreshold <= discr_over_4 <= cuspThreshold, it means the two
  // roots are within TESS_EPSILON of one another (in parametric space). This
  // is close enough for our purposes to consider them a single cusp.
  float cuspThreshold = a * (TESS_EPSILON / 2);
  cuspThreshold *= cuspThreshold;

  if (discr_over_4 < -cuspThreshold)
  {
    // The curve does not inflect or cusp. This means it might rotate more
    // than 180 degrees instead. Chop were rotation == 180 deg. (This is the
    // 2nd root where the tangent is parallel to tan0.)
    //
    //      Tangent_Direction(T) x tan0 == 0
    //      (AT^2 x tan0) + (2BT x tan0) + (C x tan0) == 0
    //      (A x C)T^2 + (2B x C)T + (C x C) == 0
    //          [[because tan0 == P1 - P0 == C]]
    //      bT^2 + 2cT + 0 == 0  [[because A x C == b, B x C == c]]
    //      T = [0, -2c/b]
    //
    // NOTE: if C == 0, then C != tan0. But this is fine because the curve
    // is definitely convex-180 if any points are colocated, and T[0] will
    // equal NaN which returns 0 chops.
    *areCusps = false;
    float root = math::ieee_float_divide(c, b_over_minus_2);
    // Is "root" inside the range [TESS_EPSILON, 1 - TESS_EPSILON)?
    if (math::bit_cast<uint32_t>(root - TESS_EPSILON) < IEEEOneMinus2Epsilon)
    {
      T[0] = root;
      return 1;
    }
    return 0;
  }

  *areCusps = discr_over_4 <= cuspThreshold;
  if (*areCusps)
  {
    // The two roots are close enough that we can consider them a single
    // cusp.
    if (a != 0 || b_over_minus_2 != 0 || c != 0)
    {
      // Pick the average of both roots.
      float root = math::ieee_float_divide(b_over_minus_2, a);
      // Is "root" inside the range [TESS_EPSILON, 1 - TESS_EPSILON)?
      if (math::bit_cast<uint32_t>(root - TESS_EPSILON) < IEEEOneMinus2Epsilon)
      {
        T[0] = root;
        return 1;
      }
      *areCusps = false;
      return 0;
    }

    // The curve is a flat line. If the points are ordered, there are no
    // inflections.
    float2 base = p3 - p0;
    float4 pX = {pts[0].x, pts[1].x, pts[2].x, pts[3].x};
    float4 pY = {pts[0].y, pts[1].y, pts[2].y, pts[3].y};
    float4 dotProds = pX * base.x + pY * base.y;
    if (simd::all(dotProds.yzw > dotProds.xyz))
    {
      // Flat line with no cusps.
      *areCusps = false;
      return 0;
    }

    // The curve is a flat line with inflections. The standard inflection
    // function doesn't detect cusps from flat lines. Find cusps by
    // searching instead for points where the tangent is perpendicular to
    // tan0. This will find any cusp point.
    //
    //     dot(tan0, Tangent_Direction(T)) == 0
    //
    //                         |T^2|
    //     tan0 * |A  2B  C| * |T  | == 0
    //            |.   .  .|   |1  |
    //
    float2 tan0 = simd::any(coeffs.C != 0.f) ? coeffs.C : p2 - p0;
    a = simd::dot(tan0, coeffs.A);
    b_over_minus_2 = -simd::dot(tan0, coeffs.B);
    c = simd::dot(tan0, coeffs.C);
    discr_over_4 = std::max(b_over_minus_2 * b_over_minus_2 - a * c, 0.f);
  }

  // Solve our quadratic equation to find where to chop. See the quadratic
  // formula from Numerical Recipes in C.
  float q = sqrtf(discr_over_4);
  q = copysignf(q, b_over_minus_2);
  q = q + b_over_minus_2;
  float2 roots = float2{q, c} / float2{a, q};

  auto inside = (roots > TESS_EPSILON) & (roots < (1 - TESS_EPSILON));
  if (inside[0])
  {
    if (inside[1] && roots[0] != roots[1])
    {
      if (roots[0] > roots[1])
      {
        roots = roots.yx; // Sort.
      }
      simd::store(T, roots);
      return 2;
    }
    T[0] = roots[0];
    return 1;
  }
  if (inside[1])
  {
    T[0] = roots[1];
    return 1;
  }
  return 0;
}

static float4 cross4(float4 ax, float4 ay, float4 bx, float4 by)
{
  float4 axby = ax * by;
  float4 aybx = ay * bx;
  return axby - aybx;
}

static float4 lengthSquared4(float4 x, float4 y)
{
  float4 xx = x * x;
  float4 yy = y * y;
  return xx + yy;
}

int4 cubicsNeedNoChops(
    float4 p0x,
    float4 p0y,
    float4 p1x,
    float4 p1y,
    float4 p2x,
    float4 p2y,
    float4 p3x,
    float4 p3y)
{
  float4 e01x = p1x - p0x;
  float4 e01y = p1y - p0y;
  float4 e12x = p2x - p1x;
  float4 e12y = p2y - p1y;
  float4 e23x = p3x - p2x;
  float4 e23y = p3y - p2y;

  float4 c01x12 = cross4(e01x, e01y, e12x, e12y);
  float4 c12x23 = cross4(e12x, e12y, e23x, e23y);
  float4 c01x23 = cross4(e01x, e01y, e23x, e23y);

  int4 e01IsZero = (e01x == 0.f) & (e01y == 0.f);
  int4 e23IsZero = (e23x == 0.f) & (e23y == 0.f);

  // If the first edge is zero, we have a triangle, and there's only one
  // relevant turn (p1-p2-p3). Duplicate it to simplify the convexity test
  // below. Same logic for if the last edge is zero. If both are zero, all of
  // the cAAxBB values become zero, causing isConvex to become false, and
  // isLine detects an actual line versus the degenerate single point case.
  c01x12 = simd::if_then_else(e01IsZero, c12x23, c01x12);
  c01x23 = simd::if_then_else(e01IsZero, c12x23, c01x23);
  c12x23 = simd::if_then_else(e23IsZero, c01x12, c12x23);
  c01x23 = simd::if_then_else(e23IsZero, c01x12, c01x23);

  float4 turnMin = simd::minFast(simd::minFast(c01x12, c12x23), c01x23);
  float4 turnMax = simd::maxFast(simd::maxFast(c01x12, c12x23), c01x23);

  // Cross products and their rounding error both scale with the squared edge
  // lengths, so the threshold scales with them too.
  float4 maxEdgeLengthSquared = simd::maxFast(
      simd::maxFast(lengthSquared4(e01x, e01y), lengthSquared4(e12x, e12y)),
      lengthSquared4(e23x, e23y));
  float4 turnThreshold = EPSILON * maxEdgeLengthSquared;

  int4 isConvex = (turnMin > turnThreshold) | (turnMax < -turnThreshold);
  int4 isLine = e01IsZero & e23IsZero & ((e12x != 0.f) | (e12y != 0.f));
  return isConvex | isLine;
}

Convex180Chops4 findCubicConvex180Chops4(
    float4 p0x,
    float4 p0y,
    float4 p1x,
    float4 p1y,
    float4 p2x,
    float4 p2y,
    float4 p3x,
    float4 p3y)
{
#ifndef RIVE_HAS_CONSTEXPR_BIT_CAST
  // Without a constexpr bit_cast, check the bits at runtime instead.
  assert(math::bit_cast<float>(IEEEOneMinus2Epsilon) == 1 - 2 * TESS_EPSILON);
#endif

  // Is each root inside [TESS_EPSILON, 1 - TESS_EPSILON)? Shifted down by
  // TESS_EPSILON, the range starts at 0, and non-negative floats order the
  // same as their bits. Negative roots and NaN have large bits, so one
  // unsigned compare rejects them too.
  const uint4 IEEEOneMinus2Epsilon4 = IEEEOneMinus2Epsilon;
  auto inChopRange = [IEEEOneMinus2Epsilon4](float4 root) {
    return math::bit_cast<uint4>(root - TESS_EPSILON) < IEEEOneMinus2Epsilon4;
  };

  // The power-basis coefficients, as in CubicCoeffs.
  float4 cx = p1x - p0x;
  float4 cy = p1y - p0y;
  float4 dx = p2x - p1x;
  float4 dy = p2y - p1y;
  float4 ex = p3x - p0x;
  float4 ey = p3y - p0y;
  float4 bx = dx - cx;
  float4 by = dy - cy;
  float4 ax = -3.f * dx + ex;
  float4 ay = -3.f * dy + ey;

  // There are inflections where F' x F'' == 0. As a quadratic:
  //
  //     F' x F'' == aT^2 + bT + c == 0.
  //
  // See:
  // https://www.microsoft.com/en-us/research/wp-content/uploads/2005/01/p1000-loop.pdf
  // Only the roots matter, so a uniform scale factor doesn't affect them.
  float4 a = cross4(ax, ay, bx, by);
  float4 b = cross4(ax, ay, cx, cy);
  float4 c = cross4(bx, by, cx, cy);
  float4 bOverMinus2 = -.5f * b;
  float4 discrOver4 = bOverMinus2 * bOverMinus2 - a * c;

  // If -cuspThreshold <= discrOver4 <= cuspThreshold, the two roots are
  // within TESS_EPSILON of each other in parametric space, close enough to
  // treat them as a single cusp.
  float4 cuspThreshold = a * (TESS_EPSILON / 2);
  cuspThreshold *= cuspThreshold;

  // Each lane takes one of three cases, but every lane computes them all.
  //
  // 1) No inflections or cusps. The curve might rotate more than 180 degrees
  // instead, so chop where the rotation reaches 180. (This is the 2nd root
  // where the tangent is parallel to tan0.)
  //
  //      Tangent_Direction(T) x tan0 == 0
  //      (AT^2 x tan0) + (2BT x tan0) + (C x tan0) == 0
  //      (A x C)T^2 + (2B x C)T + (C x C) == 0
  //          [[because tan0 == P1 - P0 == C]]
  //      bT^2 + 2cT + 0 == 0  [[because A x C == b, B x C == c]]
  //      T = [0, -2c/b]
  //
  // If C == 0, then C != tan0, but a cubic with colocated points is
  // convex-180 anyway, and the root is NaN, which doesn't chop.
  int4 noInflections = discrOver4 < -cuspThreshold;
  float4 root180 = c / bOverMinus2;
  int4 chop180 = inChopRange(root180);

  Convex180Chops4 result;
  if (simd::all(noInflections))
  {
    // Skip the divides and square root the other cases need.
    result.numChops = chop180 & 1;
    result.T0 = root180;
    result.T1 = float4(0);
    result.possibleCusps = int4(0);
    return result;
  }

  // 2) Two roots close enough to count as one cusp, or a, b and c all 0,
  // which means the control points lie on one line and may double back.
  // These are left to findCubicConvex180Chops().
  result.possibleCusps = ~noInflections & (discrOver4 <= cuspThreshold);

  // 3) Up to two inflections, as a serpentine curve has. Solve the quadratic
  // with the formula from Numerical Recipes in C.
  float4 q = simd::sqrt(discrOver4);
  q = simd::copysign(q, bOverMinus2);
  q = q + bOverMinus2;
  float4 r0 = q / a;
  float4 r1 = c / q;
  int4 in0 = (r0 > TESS_EPSILON) & (r0 < 1 - TESS_EPSILON);
  int4 in1 = (r1 > TESS_EPSILON) & (r1 < 1 - TESS_EPSILON);
  int4 twoIn = in0 & in1 & (r0 != r1);
  int4 swap = r0 > r1;
  float4 generalT0 = simd::if_then_else(
      twoIn,
      simd::if_then_else(swap, r1, r0),
      simd::if_then_else(in0, r0, r1));
  float4 generalT1 = simd::if_then_else(swap, r0, r1);
  int4 generalCount = simd::if_then_else(twoIn, int4(2), (in0 | in1) & 1);

  result.numChops = simd::if_then_else(
      noInflections,
      chop180 & 1,
      generalCount);
  result.T0 = simd::if_then_else(noInflections, root180, generalT0);
  result.T1 = generalT1;
  return result;
}
} // namespace math
} // namespace rive
