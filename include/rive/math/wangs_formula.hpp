/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Initial import from skia:src/gpu/tessellate/WangsFormula.h
 *
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/math/simd.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/math/mat2d.hpp"
#include <math.h>

#define AI RIVE_MAYBE_UNUSED RIVE_ALWAYS_INLINE

// Wang's formula gives the minimum number of evenly spaced (in the parametric sense) line segments
// that a bezier curve must be chopped into in order to guarantee all lines stay within a distance
// of "1/precision" pixels from the true curve. Its definition for a bezier curve of degree "n" is
// as follows:
//
//     maxLength = max([length(p[i+2] - 2p[i+1] + p[i]) for (0 <= i <= n-2)])
//     numParametricSegments = sqrt(maxLength * precision * n*(n - 1)/8)
//
// (Goldman, Ron. (2003). 5.6.3 Wang's Formula. "Pyramid Algorithms: A Dynamic Programming Approach
// to Curves and Surfaces for Geometric Modeling". Morgan Kaufmann Publishers.)
namespace rive
{
namespace wangs_formula
{
// Returns the value by which to multiply length in Wang's formula. (See above.)
template <int Degree> constexpr float length_term(float precision)
{
    return (Degree * (Degree - 1) / 8.f) * precision;
}
template <int Degree> constexpr float length_term_pow2(float precision)
{
    return ((Degree * Degree) * ((Degree - 1) * (Degree - 1)) / 64.f) * (precision * precision);
}

AI static float root4(float x) { return sqrtf(sqrtf(x)); }

// Returns the log2 of the provided value, were that value to be rounded up to the next power of 2.
// Returns 0 if value <= 0:
// Never returns a negative number, even if value is NaN.
//
//     sk_float_nextlog2((-inf..1]) -> 0
//     sk_float_nextlog2((1..2]) -> 1
//     sk_float_nextlog2((2..4]) -> 2
//     sk_float_nextlog2((4..8]) -> 3
//     ...
AI static int sk_float_nextlog2(float x)
{
    uint32_t bits;
    RIVE_INLINE_MEMCPY(&bits, &x, 4);
    bits += (1u << 23) - 1u; // Increment the exponent for non-powers-of-2.
    int exp = ((int32_t)bits >> 23) - 127;
    return exp & ~(exp >> 31); // Return 0 for negative or denormalized floats, and exponents < 0.
}

// Returns nextlog2(sqrt(x)):
//
//   log2(sqrt(x)) == log2(x^(1/2)) == log2(x)/2 == log2(x)/log2(4) == log4(x)
//
AI static int nextlog4(float x) { return (sk_float_nextlog2(x) + 1) >> 1; }

// Returns nextlog2(sqrt(sqrt(x))):
//
//   log2(sqrt(sqrt(x))) == log2(x^(1/4)) == log2(x)/4 == log2(x)/log2(16) == log16(x)
//
AI static int nextlog16(float x) { return (sk_float_nextlog2(x) + 3) >> 2; }

// Represents the upper-left 2x2 matrix of an affine transform for applying to vectors:
//
//     VectorXform(p1 - p0) == M * float3(p1, 1) - M * float3(p0, 1)
//
class alignas(32) VectorXform
{
public:
    AI VectorXform() : m_scale(1), m_skew(0) {}
    AI explicit VectorXform(const Mat2D& m) { *this = m; }

    AI VectorXform& operator=(const Mat2D& m)
    {
        m_scale = float2{m[0], m[3]}.xyxy;
        m_skew = simd::load2f(&m[1]).yxyx;
        return *this;
    }

    AI float2 operator()(float2 vector) const
    {
        return m_scale.xy * vector + m_skew.xy * vector.yx;
    }
    AI float4 operator()(float4 vectors) const { return m_scale * vectors + m_skew * vectors.yxwz; }

private:
    float4 m_scale;
    float4 m_skew;
};

// Returns Wang's formula, raised to the 4th power, specialized for a quadratic curve.
AI static float quadratic_pow4(float2 p0,
                               float2 p1,
                               float2 p2,
                               float precision,
                               const VectorXform& vectorXform = VectorXform())
{
    float2 v = -2.f * p1 + p0 + p2;
    v = vectorXform(v);
    float2 vv = v * v;
    return (vv[0] + vv[1]) * length_term_pow2<2>(precision);
}
AI static float quadratic_pow4(const Vec2D pts[],
                               float precision,
                               const VectorXform& vectorXform = VectorXform())
{
    return quadratic_pow4(simd::load2f(&pts[0].x),
                          simd::load2f(&pts[1].x),
                          simd::load2f(&pts[2].x),
                          precision,
                          vectorXform);
}

// Returns Wang's formula specialized for a quadratic curve.
AI static float quadratic(const Vec2D pts[],
                          float precision,
                          const VectorXform& vectorXform = VectorXform())
{
    return root4(quadratic_pow4(pts, precision, vectorXform));
}

// Returns the log2 value of Wang's formula specialized for a quadratic curve, rounded up to the
// next int.
AI static int quadratic_log2(const Vec2D pts[],
                             float precision,
                             const VectorXform& vectorXform = VectorXform())
{
    // nextlog16(x) == ceil(log2(sqrt(sqrt(x))))
    return nextlog16(quadratic_pow4(pts, precision, vectorXform));
}

// Returns Wang's formula, raised to the 4th power, specialized for a cubic curve.
AI static float cubic_pow4(const Vec2D pts[],
                           float precision,
                           const VectorXform& vectorXform = VectorXform())
{
    float4 p01 = simd::load4f(pts);
    float4 p12 = simd::load4f(pts + 1);
    float4 p23 = simd::load4f(pts + 2);
    float4 v = -2.f * p12 + p01 + p23;
    v = vectorXform(v);
    float4 vv = v * v;
    return std::max(vv[0] + vv[1], vv[2] + vv[3]) * length_term_pow2<3>(precision);
}

// Returns Wang's formula specialized for a cubic curve.
AI static float cubic(const Vec2D pts[],
                      float precision,
                      const VectorXform& vectorXform = VectorXform())
{
    return root4(cubic_pow4(pts, precision, vectorXform));
}

// Returns the log2 value of Wang's formula specialized for a cubic curve, rounded up to the next
// int.
AI static int cubic_log2(const Vec2D pts[],
                         float precision,
                         const VectorXform& vectorXform = VectorXform())
{
    // nextlog16(x) == ceil(log2(sqrt(sqrt(x))))
    return nextlog16(cubic_pow4(pts, precision, vectorXform));
}

// Returns the maximum number of line segments a cubic with the given device-space bounding box size
// would ever need to be divided into, raised to the 4th power. This is simply a special case of the
// cubic formula where we maximize its value by placing control points on specific corners of the
// bounding box.
AI static float worst_case_cubic_pow4(float devWidth, float devHeight, float precision)
{
    float kk = length_term_pow2<3>(precision);
    return 4 * kk * (devWidth * devWidth + devHeight * devHeight);
}

// Returns the maximum number of line segments a cubic with the given device-space bounding box size
// would ever need to be divided into.
AI static float worst_case_cubic(float devWidth, float devHeight, float precision)
{
    return root4(worst_case_cubic_pow4(devWidth, devHeight, precision));
}

// Returns the maximum log2 number of line segments a cubic with the given device-space bounding box
// size would ever need to be divided into.
AI static int worst_case_cubic_log2(float devWidth, float devHeight, float precision)
{
    // nextlog16(x) == ceil(log2(sqrt(sqrt(x))))
    return nextlog16(worst_case_cubic_pow4(devWidth, devHeight, precision));
}

// Returns Wang's formula specialized for a conic curve, raised to the second power.
// Input points should be in projected space.
//
// This is not actually due to Wang, but is an analogue from (Theorem 3, corollary 1):
//   J. Zheng, T. Sederberg. "Estimating Tessellation Parameter Intervals for
//   Rational Curves and Surfaces." ACM Transactions on Graphics 19(1). 2000.
AI static float conic_pow2(float precision,
                           float2 p0,
                           float2 p1,
                           float2 p2,
                           float w,
                           const VectorXform& vectorXform = VectorXform())
{
    p0 = vectorXform(p0);
    p1 = vectorXform(p1);
    p2 = vectorXform(p2);

    // Compute center of bounding box in projected space
    const float2 C = 0.5f * (simd::min(simd::min(p0, p1), p2) + simd::max(simd::max(p0, p1), p2));

    // Translate by -C. This improves translation-invariance of the formula,
    // see Sec. 3.3 of cited paper
    p0 -= C;
    p1 -= C;
    p2 -= C;

    // Compute max length
    const float max_len =
        sqrtf(std::max(simd::dot(p0, p0), std::max(simd::dot(p1, p1), simd::dot(p2, p2))));

    // Compute forward differences
    const float2 dp = -2.f * w * p1 + p0 + p2;
    const float dw = fabsf(-2.f * w + 2);

    // Compute numerator and denominator for parametric step size of linearization. Here, the
    // epsilon referenced from the cited paper is 1/precision.
    const float rp_minus_1 = std::max(0.f, max_len * precision - 1);
    const float numer = sqrtf(simd::dot(dp, dp)) * precision + rp_minus_1 * dw;
    const float denom = 4 * std::min(w, 1.f);

    // Number of segments = sqrt(numer / denom).
    // This assumes parametric interval of curve being linearized is [t0,t1] = [0, 1].
    // If not, the number of segments is (tmax - tmin) / sqrt(denom / numer).
    return numer / denom;
}
AI static float conic_pow2(float precision,
                           const Vec2D pts[],
                           float w,
                           const VectorXform& vectorXform = VectorXform())
{
    return conic_pow2(precision,
                      simd::load2f(&pts[0].x),
                      simd::load2f(&pts[1].x),
                      simd::load2f(&pts[2].x),
                      w,
                      vectorXform);
}

// Returns the value of Wang's formula specialized for a conic curve.
AI static float conic(float tolerance,
                      const Vec2D pts[],
                      float w,
                      const VectorXform& vectorXform = VectorXform())
{
    return sqrtf(conic_pow2(tolerance, pts, w, vectorXform));
}

// Returns the log2 value of Wang's formula specialized for a conic curve, rounded up to the next
// int.
AI static int conic_log2(float tolerance,
                         const Vec2D pts[],
                         float w,
                         const VectorXform& vectorXform = VectorXform())
{
    // nextlog4(x) == ceil(log2(sqrt(x)))
    return nextlog4(conic_pow2(tolerance, pts, w, vectorXform));
}
} // namespace wangs_formula
} // namespace rive

#undef AI
