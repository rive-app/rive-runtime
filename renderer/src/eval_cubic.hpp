/*
 * Copyright 2024 Rive
 */

#pragma once

#include "rive/math/simd.hpp"
#include "rive/math/vec2d.hpp"

namespace rive::gpu
{
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
        // Find the cubic's power basis coefficients. These define the bezier curve as:
        //
        //                                  |t^3|
        //     Cubic(T) = x,y = |A  B  C| * |t^2| + P0
        //                      |.  .  .|   |t  |
        //
        // (Duplicate coefficients across a float4 so we can evaluate two at once.)
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

    // Evaluates [Xa, Ya, Xb, Yb] at locations [Ta, Ta, Tb, Tb].
    inline float4 at(float4 t) const
    {
        return t * (t * (t * m_A + m_B) + m_C) + m_P0; // At^3 + Bt^2 + Ct + P0
    }

private:
    float4 m_A;
    float4 m_B;
    float4 m_C;
    float4 m_P0;
};
} // namespace rive::gpu
