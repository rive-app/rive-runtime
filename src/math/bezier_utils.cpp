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
Vec2D eval_cubic_at(const Vec2D p[4], float t)
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

void chop_cubic_at(const Vec2D src[4], Vec2D dst[7], float t)
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

void chop_cubic_at(const Vec2D src[4], Vec2D dst[10], float t0, float t1)
{
    assert(0 <= t0 && t0 <= t1 && t1 <= 1);

    if (t1 == 1)
    {
        chop_cubic_at(src, dst, t0);
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

void chop_cubic_at(const Vec2D src[4],
                   Vec2D dst[],
                   const float tValues[],
                   int tCount)
{
    assert(tValues == nullptr ||
           std::all_of(tValues, tValues + tCount, [](float t) {
               return t >= 0 && t <= 1;
           }));
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
                    tt = simd::clamp((tt - lastT) / (1 - lastT),
                                     float2(0),
                                     float2(1));
                    lastT = tValues[i + 1];
                }
                else
                {
                    tt = float2{1, 2} / static_cast<float>(tCount + 1 - i);
                }
                chop_cubic_at(src, dst, tt[0], tt[1]);
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
                chop_cubic_at(src, dst, t);
            }
        }
    }
}

float measure_angle_between_vectors(Vec2D a, Vec2D b)
{
    float cosTheta =
        math::ieee_float_divide(Vec2D::dot(a, b),
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

int find_cubic_convex_180_chops(const Vec2D pts[], float T[2], bool* areCusps)
{
    assert(pts);
    assert(T);
    assert(areCusps);

    // Floating-point representation of "1 - 2*TESS_EPSILON".
    constexpr static uint32_t kIEEE_one_minus_2_epsilon =
        (127 << 23) - 2 * (1 << (24 - 10));
    // Unfortunately we don't have a way to static_assert this, but we can
    // runtime assert that the kIEEE_one_minus_2_epsilon bits are correct.
    assert(math::bit_cast<float>(kIEEE_one_minus_2_epsilon) ==
           1 - 2 * TESS_EPSILON);

    float2 p0 = simd::load2f(&pts[0].x);
    float2 p1 = simd::load2f(&pts[1].x);
    float2 p2 = simd::load2f(&pts[2].x);
    float2 p3 = simd::load2f(&pts[3].x);
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
        if (math::bit_cast<uint32_t>(root - TESS_EPSILON) <
            kIEEE_one_minus_2_epsilon)
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
            if (math::bit_cast<uint32_t>(root - TESS_EPSILON) <
                kIEEE_one_minus_2_epsilon)
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

int find_cubic_convex_90_chops(const Vec2D pts[],
                               float outT[4],
                               float cuspPadding,
                               bool* areCusps)
{
    assert(pts);
    assert(outT);
    assert(areCusps);

    // Now find the cubic's inflection function.
    // There are inflections where F' x F'' == 0.
    //
    // We formulate this as a quadratic equation:
    //
    //     F' x F'' == a * T^2 + b * T + c == 0.
    //
    // See:
    // https://www.microsoft.com/en-us/research/wp-content/uploads/2005/01/p1000-loop.pdf
    // NOTE: We only need the roots, so a uniform scale factor does not affect
    // the solution.
    CubicCoeffs coeffs(pts);
    float a = simd::cross(coeffs.A, coeffs.B);
    float b_over_2 = simd::cross(coeffs.A, coeffs.C) * .5f;
    float c = simd::cross(coeffs.B, coeffs.C);
    float discr_over_4 = b_over_2 * b_over_2 - a * c;

    // If -cuspThreshold <= discr_over_4 <= cuspThreshold, it means the two
    // roots are within TESS_EPSILON of one another (in parametric space). This
    // is close enough for our purposes to consider them a single cusp.
    float cuspThreshold = a * (TESS_EPSILON / 2);
    cuspThreshold *= cuspThreshold;

    // Find the first two chops, based on curve classification. Also fill in
    // "tan90", which will define the second pair of chops as the two points
    // perpendicular to "tan90".
    float4 T;
    float2 tan90;
    if (discr_over_4 < -cuspThreshold ||
        // Check if it's quadratic.
        std::max(fabs(a), fabs(b_over_2)) < fabs(c) * TESS_EPSILON)
    {
        // The curve is a loop or quadratic.
        // One chop is where rotation == 180 deg (which happens at infinity if
        // the curve is quadratic).
        // (This is the 2nd root where the tangent is parallel to tan0.)
        //
        //    Tangent_Direction(T) x tan0 == 0
        //    (AT^2 x tan0) + (2BT x tan0) + (C x tan0) == 0
        //    (A x C)T^2 + (2B x C)T + (C x C) == 0
        //        [[because tan0 == P1 - P0 == C]]
        //    bT^2 + 2cT + 0 == 0  [[because A x C == b, B x C == c]]
        //    T = [0, -2c/b]
        //
        // NOTE: if C == 0, then C != tan0. But this is fine because the curve
        // can only rotate 180 degrees if the endpoints are colocated, and this
        // gets handled next.
        T.xy = {-c / b_over_2, 1};

        // Next chop 90 degrees from the starting tangent of the curve.
        tan90 = simd::any(coeffs.C != 0.f)
                    ? coeffs.C
                    : math::bit_cast<float2>(pts[2] - pts[0]);
        *areCusps = false;
    }
    else if (discr_over_4 > cuspThreshold)
    {
        // The curve is serpentine. Solve for the two inflection points.
        float q = sqrtf(discr_over_4);
        q = -b_over_2 - copysignf(q, b_over_2);
        T.xy = float2{q, c} / float2{a, q};

        // Next chop 90 degrees from the whichever inflection point is closest
        // to the middle.
        float t = fabsf(T.x - .5f) < fabsf(T.y - .5f) ? T.x : T.y;
        tan90 = (coeffs.A * t + 2.f * coeffs.B) * t + coeffs.C;
        *areCusps = false;
    }
    else
    {
        // The curve is a cusp. A proper cusp is at T=-b/2a, but just solving
        // for 90 degrees from the starting tangent will also find it, in
        // addition to finding cusps from degenerate flat lines reversing
        // direction. Since 180 degrees of rotation is lost to the cusp, we only
        // need to find 2 roots max.
        T.xy = 1;
        tan90 = simd::any(coeffs.C != 0.f)
                    ? coeffs.C
                    : math::bit_cast<float2>(pts[2] - pts[0]);
        *areCusps = true;
    }

    // Find a second set of chops where the curve is perpendicular to tan90.
    //
    //   Tangent_Direction(T) dot tan90 == 0
    //   (A dot tan90) * T^2 + (2B dot tan90) * T + (C dot tan90) == 0
    //
    a = simd::dot(coeffs.A, tan90);
    b_over_2 = simd::dot(coeffs.B, tan90);
    c = simd::dot(coeffs.C, tan90);
    discr_over_4 = b_over_2 * b_over_2 - a * c;
    float q = sqrtf(discr_over_4);
    q = -b_over_2 - copysignf(q, b_over_2);
    T.zw = float2{q, c} / float2{a, q};

    // Throw out T <= epsilon and T >= epsilon by converting them to 1.
    // (Use logic such that NaN also converts to 1.)
    T = simd::if_then_else((T > 0) & (T < 1), T, float4(1));
    assert(simd::all(T > 0));
    assert(simd::all(T <= 1));

    // Sort the roots.
    T = simd::if_then_else((float2{T.x, T.z} < float2{T.y, T.w}).xxyy,
                           T,
                           T.yxwz);
    T = simd::if_then_else((float2{T.x, T.y} < float2{T.z, T.w}).xyxy,
                           T,
                           T.zwxy);
    T = T.y < T.z ? T : T.xzyw;

    // Count the number of roots that != 1 and store T.
    int4 n4 = (T != 1) & 1;
    n4.xy += n4.zw;
    int n = n4.x + n4.y;
    simd::store(outT, T);

    if (*areCusps && n > 0)
    {
        // Generate padding around cusp points. Odd numbered chops are always
        // padding sections that pass through a cusp.
        assert(n <= 2);
        for (int i = n - 1; i >= 0; --i)
        {
            float maxT = i == n - 1 ? 1 : outT[i * 2 + 1];
            float minT = i == 0 ? 0 : (outT[i - 1] + outT[i]) * .5f;
            outT[i * 2 + 1] = std::min(outT[i] + cuspPadding, maxT);
            outT[i * 2 + 0] = std::max(outT[i] - cuspPadding, minT);
        }
        n *= 2;
        // Re-clip and re-sort n after adding cusp padding. This is a hack, but
        // we leave it here for now becase we're about to remove this entire
        // method in favor of something more robust.
        if (outT[n - 1] == 1)
        {
            --n;
        }
        std::sort(outT, outT + n);
    }

    return n;
}

float find_cubic_max_height(const Vec2D p[4], float* outT)
{
    // Calculate the cubic height function: 3(dht^3 - (h1 + dh)t^2 + h1t)
    Vec2D n = (p[3] - p[0]).normalized();
    n = {-n.y, n.x};
    float h2 = Vec2D::dot(n, p[2] - p[0]);
    float h1 = Vec2D::dot(n, p[1] - p[0]);
    float dh = h1 - h2;

    // A cubic's height function has two maxima. Find both.
    float a = 3 * dh;
    float b_over_minus_2 = dh + h1;
    float c = h1;
    float q = sqrtf(std::max(dh * dh + h2 * h1, 0.f));
    q = b_over_minus_2 + copysignf(q, b_over_minus_2);
    float2 tt = float2{q, c} / float2{a, q};
    tt = simd::clamp(tt, float2{0, 0}, float2{1, 1});
    float2 hh = 3.f * (tt * (tt * (tt * dh - (h1 + dh)) + h1));

    // Go with whichever maximum is larger.
    hh = simd::abs(hh);
    if (outT != nullptr)
        *outT = hh.x > hh.y ? tt.x : tt.y;
    return fmaxf(hh.x, hh.y);
}

float measure_cubic_local_curvature(const Vec2D p[4],
                                    const math::CubicCoeffs& coeffs,
                                    float T,
                                    float desiredSpread)
{
    float2 tan = 3.f * (((coeffs.A * T) + 2.f * coeffs.B) * T + coeffs.C);
    float lengthTan = sqrtf(simd::dot(tan, tan));
    if (lengthTan == 0)
    {
        return 0;
    }

    // Define the function
    //
    //    Spread(dt) = A__*dt^3 + C__*dt
    //
    // Which calculates the spread of the curve in local coordinates, parallel
    // to tan, over the range "T - dt .. T + dt".
    tan *= 1 / lengthTan;
    float A__ = 2 * simd::dot(coeffs.A, tan);
    float C__ = 3 * (A__ * T + 4 * simd::dot(coeffs.B, tan)) * T +
                6 * simd::dot(coeffs.C, tan);

    // Decide the "targetSpread" across which we will measure curvature. Ideally
    // this is "desiredSpread", but use less than that if that would reach
    // outside T=0..1.
    float maxDT = fminf(T, 1 - T);
    float maxSpread = (A__ * maxDT * maxDT + C__) * maxDT;
    // Pad the maxSpread to guarantee we won't step outside T=0..1.
    float targetSpread = fminf(desiredSpread, maxSpread * .9999f);

    // Solve for dt, where Spread(dt) == targetSpread.
    float dt;
    if (A__ == 0)
    {
        // Degenerate case: Spread(dt) == C__*dt.
        dt = targetSpread / C__;
    }
    else
    {
        // Solve the normalized cubic x^3 + ax^2 + bx + c == 0.
        // (Numerical Recipes in C, 5.6 Quadratic and Cubic Equations,
        // https://hd.fizyka.umk.pl/~jacek/docs/nrc/c5-6.pdf)
        float r = 1 / A__;
        float /*a = 0,*/ b = C__ * r, c = -targetSpread * r;
        float Q = (-1.f / 3) * b, R = .5f * c;
        float discr = R * R - Q * Q * Q;
        if (discr < 0)
        {
            float sqrtQ = sqrtf(Q);
            float theta = acosf(R / (sqrtQ * sqrtQ * sqrtQ));
            // The 3 roots are: (because a == 0)
            //   -2 * sqrt(Q) * cos(theta/3 + float3{0, 1, -1} * 2*pi/3)
            // We want the root closest to zero, which will be the 3rd root
            // because its argument for cos() is always closest to +-pi/2.
            dt = -2 * sqrtQ * cosf(theta * (1.f / 3) + (-math::PI * 2 / 3));
        }
        else
        {
            float A = -copysignf(cbrtf(fabsf(R) + sqrtf(discr)), R);
            dt = A != 0 ? A + Q / A : 0;
        }
    }
    dt = fabsf(dt);

    // Measure curvature over the spread T - dt .. T + dt.
    float4 t0011 = T + float4{-dt, -dt, dt, dt};
    float4 tanDirs =
        (coeffs.A.xyxy * t0011 + 2.f * coeffs.B.xyxy) * t0011 + coeffs.C.xyxy;
    Vec2D tan0 = math::bit_cast<Vec2D>(tanDirs.xy);
    Vec2D tan1 = math::bit_cast<Vec2D>(tanDirs.zw);
    if (t0011.x < 1e-3f) // Calculate a more stable tangent at T <= 0 in case
    {                    // we've encountered a cusp.
        tan0 = (p[0] != p[1] ? p[1] : p[1] != p[2] ? p[2] : p[3]) - p[0];
    }
    if (t0011.z > 1 - 1e-3f) // Calculate a more stable tangent at T >= 1 in
    {                        // case we've encountered a cusp.
        tan1 = p[3] - (p[3] != p[2] ? p[2] : p[2] != p[1] ? p[1] : p[0]);
    }
    // NOTE: this will not capture the total absolute curvature if there is an
    // inflection point, but it's arguably what we want anyway since this will
    // return the composite curvature over the spread (i.e., clockwise curvature
    // minus counterclockwise).
    return math::measure_angle_between_vectors(tan0, tan1);
}
} // namespace math
} // namespace rive
