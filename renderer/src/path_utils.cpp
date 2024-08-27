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

#include "path_utils.hpp"

#include "rive/math/math_types.hpp"
#include "rive/math/simd.hpp"

namespace rive::pathutils
{
Vec2D EvalCubicAt(const Vec2D p[4], float t)
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

void ChopCubicAt(const Vec2D src[4], Vec2D dst[7], float t)
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

void ChopCubicAt(const Vec2D src[4], Vec2D dst[10], float t0, float t1)
{
    assert(0 <= t0 && t0 <= t1 && t1 <= 1);

    if (t1 == 1)
    {
        ChopCubicAt(src, dst, t0);
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

void ChopCubicAt(const Vec2D src[4], Vec2D dst[], const float tValues[], int tCount)
{
    assert(tValues == nullptr ||
           std::all_of(tValues, tValues + tCount, [](float t) { return t >= 0 && t <= 1; }));
    assert(tValues == nullptr || std::is_sorted(tValues, tValues + tCount));

    if (dst)
    {
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
                ChopCubicAt(src, dst, tt[0], tt[1]);
                src = dst = dst + 6;
            }
            if (i < tCount)
            {
                // Chop the final cubic if there was an odd number of chops.
                assert(i + 1 == tCount);
                float t = tValues != nullptr ? tValues[i] : .5f;
                t = simd::clamp<float, 1>(math::ieee_float_divide(t - lastT, 1 - lastT), 0, 1).x;
                ChopCubicAt(src, dst, t);
            }
        }
    }
}

float MeasureAngleBetweenVectors(Vec2D a, Vec2D b)
{
    float cosTheta =
        math::ieee_float_divide(Vec2D::dot(a, b), sqrtf(Vec2D::dot(a, a) * Vec2D::dot(b, b)));
    // Pin cosTheta such that if it is NaN (e.g., if a or b was 0), then we return acos(1) = 0.
    cosTheta = std::max(std::min(1.f, cosTheta), -1.f);
    return acosf(cosTheta);
}

int FindCubicConvex180Chops(const Vec2D pts[], float T[2], bool* areCusps)
{
    assert(pts);
    assert(T);
    assert(areCusps);

    // If a chop falls within a distance of "kEpsilon" from 0 or 1, throw it out. Tangents become
    // unstable when we chop too close to the boundary. This works out because the tessellation
    // shaders don't allow more than 2^10 parametric segments, and they snap the beginning and
    // ending edges at 0 and 1. So if we overstep an inflection or point of 180-degree rotation by a
    // fraction of a tessellation segment, it just gets snapped.
    constexpr static float kEpsilon = 1.f / (1 << 10);
    // Floating-point representation of "1 - 2*kEpsilon".
    constexpr static uint32_t kIEEE_one_minus_2_epsilon = (127 << 23) - 2 * (1 << (24 - 10));
    // Unfortunately we don't have a way to static_assert this, but we can runtime assert that the
    // kIEEE_one_minus_2_epsilon bits are correct.
    assert(math::bit_cast<float>(kIEEE_one_minus_2_epsilon) == 1 - 2 * kEpsilon);

    float2 p0 = simd::load2f(&pts[0].x);
    float2 p1 = simd::load2f(&pts[1].x);
    float2 p2 = simd::load2f(&pts[2].x);
    float2 p3 = simd::load2f(&pts[3].x);

    // Find the cubic's power basis coefficients. These define the bezier curve as:
    //
    //                                    |T^3|
    //     Cubic(T) = x,y = |A  3B  3C| * |T^2| + P0
    //                      |.   .   .|   |T  |
    //
    // And the tangent direction (scaled by a uniform 1/3) will be:
    //
    //                                                 |T^2|
    //     Tangent_Direction(T) = dx,dy = |A  2B  C| * |T  |
    //                                    |.   .  .|   |1  |
    //
    float2 C = p1 - p0;
    float2 D = p2 - p1;
    float2 E = p3 - p0;
    float2 B = D - C;
    float2 A = -3.f * D + E;

    // Now find the cubic's inflection function. There are inflections where F' x F'' == 0.
    // We formulate this as a quadratic equation:  F' x F'' == aT^2 + bT + c == 0.
    // See: https://www.microsoft.com/en-us/research/wp-content/uploads/2005/01/p1000-loop.pdf
    // NOTE: We only need the roots, so a uniform scale factor does not affect the solution.
    float a = simd::cross(A, B);
    float b = simd::cross(A, C);
    float c = simd::cross(B, C);
    float b_over_minus_2 = -.5f * b;
    float discr_over_4 = b_over_minus_2 * b_over_minus_2 - a * c;

    // If -cuspThreshold <= discr_over_4 <= cuspThreshold, it means the two roots are within
    // kEpsilon of one another (in parametric space). This is close enough for our purposes to
    // consider them a single cusp.
    float cuspThreshold = a * (kEpsilon / 2);
    cuspThreshold *= cuspThreshold;

    if (discr_over_4 < -cuspThreshold)
    {
        // The curve does not inflect or cusp. This means it might rotate more than 180 degrees
        // instead. Chop were rotation == 180 deg. (This is the 2nd root where the tangent is
        // parallel to tan0.)
        //
        //      Tangent_Direction(T) x tan0 == 0
        //      (AT^2 x tan0) + (2BT x tan0) + (C x tan0) == 0
        //      (A x C)T^2 + (2B x C)T + (C x C) == 0  [[because tan0 == P1 - P0 == C]]
        //      bT^2 + 2cT + 0 == 0  [[because A x C == b, B x C == c]]
        //      T = [0, -2c/b]
        //
        // NOTE: if C == 0, then C != tan0. But this is fine because the curve is definitely
        // convex-180 if any points are colocated, and T[0] will equal NaN which returns 0 chops.
        *areCusps = false;
        float root = math::ieee_float_divide(c, b_over_minus_2);
        // Is "root" inside the range [kEpsilon, 1 - kEpsilon)?
        if (math::bit_cast<uint32_t>(root - kEpsilon) < kIEEE_one_minus_2_epsilon)
        {
            T[0] = root;
            return 1;
        }
        return 0;
    }

    *areCusps = (discr_over_4 <= cuspThreshold);
    if (*areCusps)
    {
        // The two roots are close enough that we can consider them a single cusp.
        if (a != 0 || b_over_minus_2 != 0 || c != 0)
        {
            // Pick the average of both roots.
            float root = math::ieee_float_divide(b_over_minus_2, a);
            // Is "root" inside the range [kEpsilon, 1 - kEpsilon)?
            if (math::bit_cast<uint32_t>(root - kEpsilon) < kIEEE_one_minus_2_epsilon)
            {
                T[0] = root;
                return 1;
            }
            return 0;
        }

        // The curve is a flat line. The standard inflection function doesn't detect cusps from flat
        // lines. Find cusps by searching instead for points where the tangent is perpendicular to
        // tan0. This will find any cusp point.
        //
        //     dot(tan0, Tangent_Direction(T)) == 0
        //
        //                         |T^2|
        //     tan0 * |A  2B  C| * |T  | == 0
        //            |.   .  .|   |1  |
        //
        float2 tan0 = simd::any(C != 0.f) ? C : p2 - p0;
        a = simd::dot(tan0, A);
        b_over_minus_2 = -simd::dot(tan0, B);
        c = simd::dot(tan0, C);
        discr_over_4 = std::max(b_over_minus_2 * b_over_minus_2 - a * c, 0.f);
    }

    // Solve our quadratic equation to find where to chop. See the quadratic formula from
    // Numerical Recipes in C.
    float q = sqrtf(discr_over_4);
    q = copysignf(q, b_over_minus_2);
    q = q + b_over_minus_2;
    float2 roots = float2{q, c} / float2{a, q};

    auto inside = (roots > kEpsilon) & (roots < (1 - kEpsilon));
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

#if 0
namespace
{
// This value only protects us against getting stuck in infinite recursion due to fp32 precision
// issues. Mathematically, every curve should reduce to manageable visible sections in O(log N)
// chops, where N is the the magnitude of its control points.
//
// But, to define a protective upper bound, a cubic can enter or exit the viewport as many as 6
// times. So we may need to refine the curve (via binary search chopping at T=.5) up to 6 times.
//
// Furthermore, chopping a cubic at T=.5 may only reduce its length by 1/8 (.5^3), so we may require
// up to 6 chops in order to reduce the length by 1/2.
constexpr static int kMaxChopsPerCurve = 128 /*magnitude of +fp32_max - -fp32_max*/ *
                                         6 /*max number of chops to reduce the length by half*/ *
                                         6 /*max number of viewport boundary crosses*/;

// Writes a new path, chopping as necessary so no verbs require more segments than
// kMaxTessellationSegmentsPerCurve. Curves completely outside the viewport are flattened into
// lines.
class PathChopper
{
public:
    PathChopper(float tessellationPrecision, const SkMatrix& matrix, const SkRect& viewport) :
        fTessellationPrecision(tessellationPrecision),
        fCullTest(viewport, matrix),
        fVectorXform(matrix)
    {
        fPath.setIsVolatile(true);
    }

    SkPath path() const { return fPath; }

    void moveTo(SkPoint p) { fPath.moveTo(p); }
    void lineTo(const SkPoint p[2]) { fPath.lineTo(p[1]); }
    void close() { fPath.close(); }

    void quadTo(const SkPoint quad[3])
    {
        assert(fPointStack.empty());
        // Use a heap stack to recursively chop the quad into manageable, on-screen segments.
        fPointStack.push_back_n(3, quad);
        int numChops = 0;
        while (!fPointStack.empty())
        {
            const SkPoint* p = fPointStack.end() - 3;
            if (!fCullTest.areVisible3(p))
            {
                fPath.lineTo(p[2]);
            }
            else
            {
                float n4 = wangs_formula::quadratic_p4(fTessellationPrecision, p, fVectorXform);
                if (n4 > kMaxSegmentsPerCurve_p4 && numChops < kMaxChopsPerCurve)
                {
                    SkPoint chops[5];
                    SkChopQuadAtHalf(p, chops);
                    fPointStack.pop_back_n(3);
                    fPointStack.push_back_n(3, chops + 2);
                    fPointStack.push_back_n(3, chops);
                    ++numChops;
                    continue;
                }
                fPath.quadTo(p[1], p[2]);
            }
            fPointStack.pop_back_n(3);
        }
    }

    void conicTo(const SkPoint conic[3], float weight)
    {
        assert(fPointStack.empty());
        assert(fWeightStack.empty());
        // Use a heap stack to recursively chop the conic into manageable, on-screen segments.
        fPointStack.push_back_n(3, conic);
        fWeightStack.push_back(weight);
        int numChops = 0;
        while (!fPointStack.empty())
        {
            const SkPoint* p = fPointStack.end() - 3;
            float w = fWeightStack.back();
            if (!fCullTest.areVisible3(p))
            {
                fPath.lineTo(p[2]);
            }
            else
            {
                float n2 = wangs_formula::conic_p2(fTessellationPrecision, p, w, fVectorXform);
                if (n2 > kMaxSegmentsPerCurve_p2 && numChops < kMaxChopsPerCurve)
                {
                    SkConic chops[2];
                    if (!SkConic(p, w).chopAt(.5, chops))
                    {
                        SkPoint line[2] = {p[0], p[2]};
                        this->lineTo(line);
                        continue;
                    }
                    fPointStack.pop_back_n(3);
                    fWeightStack.pop_back();
                    fPointStack.push_back_n(3, chops[1].fPts);
                    fWeightStack.push_back(chops[1].fW);
                    fPointStack.push_back_n(3, chops[0].fPts);
                    fWeightStack.push_back(chops[0].fW);
                    ++numChops;
                    continue;
                }
                fPath.conicTo(p[1], p[2], w);
            }
            fPointStack.pop_back_n(3);
            fWeightStack.pop_back();
        }
        assert(fWeightStack.empty());
    }

    void cubicTo(const SkPoint cubic[4])
    {
        assert(fPointStack.empty());
        // Use a heap stack to recursively chop the cubic into manageable, on-screen segments.
        fPointStack.push_back_n(4, cubic);
        int numChops = 0;
        while (!fPointStack.empty())
        {
            SkPoint* p = fPointStack.end() - 4;
            if (!fCullTest.areVisible4(p))
            {
                fPath.lineTo(p[3]);
            }
            else
            {
                float n4 = wangs_formula::cubic_p4(fTessellationPrecision, p, fVectorXform);
                if (n4 > kMaxSegmentsPerCurve_p4 && numChops < kMaxChopsPerCurve)
                {
                    SkPoint chops[7];
                    SkChopCubicAtHalf(p, chops);
                    fPointStack.pop_back_n(4);
                    fPointStack.push_back_n(4, chops + 3);
                    fPointStack.push_back_n(4, chops);
                    ++numChops;
                    continue;
                }
                fPath.cubicTo(p[1], p[2], p[3]);
            }
            fPointStack.pop_back_n(4);
        }
    }

private:
    const float fTessellationPrecision;
    const CullTest fCullTest;
    const wangs_formula::VectorXform fVectorXform;
    SkPath fPath;

    // Used for stack-based recursion (instead of using the runtime stack).
    SkSTArray<8, SkPoint> fPointStack;
    SkSTArray<2, float> fWeightStack;
};
} // namespace

SkPath PreChopPathCurves(float tessellationPrecision,
                         const SkPath& path,
                         const SkMatrix& matrix,
                         const SkRect& viewport)
{
    // If the viewport is exceptionally large, we could end up blowing out memory with an unbounded
    // number of of chops. Therefore, we require that the viewport is manageable enough that a fully
    // contained curve can be tessellated in kMaxTessellationSegmentsPerCurve or fewer. (Any larger
    // and that amount of pixels wouldn't fit in memory anyway.)
    assert(wangs_formula::worst_case_cubic(tessellationPrecision,
                                             viewport.width(),
                                             viewport.height()) <= kMaxSegmentsPerCurve);
    PathChopper chopper(tessellationPrecision, matrix, viewport);
    for (auto [verb, p, w] : SkPathPriv::Iterate(path))
    {
        switch (verb)
        {
            case SkPathVerb::kMove:
                chopper.moveTo(p[0]);
                break;
            case SkPathVerb::kLine:
                chopper.lineTo(p);
                break;
            case SkPathVerb::kQuad:
                chopper.quadTo(p);
                break;
            case SkPathVerb::kConic:
                chopper.conicTo(p, *w);
                break;
            case SkPathVerb::kCubic:
                chopper.cubicTo(p);
                break;
            case SkPathVerb::kClose:
                chopper.close();
                break;
        }
    }
    return chopper.path();
}
#endif
} // namespace rive::pathutils
