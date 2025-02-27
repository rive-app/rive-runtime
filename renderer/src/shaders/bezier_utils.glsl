/*
 * Copyright 2020 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Some initial imports from
 * skia:src/gpu/ganesh/tessellate/GrStrokeTessellationShader.cpp
 *
 * Copyright 2025 Rive
 */

// Common definitions and math functions for working with bezier curves.

// make_float4 and make_float2 are defined for compatibility with C++, so we can
// compile this file as C++ and unit test it.
#ifndef make_float4
#define make_float4 float4
#endif
#ifndef make_float2
#define make_float2 float2
#endif

INLINE float cosine_between_vectors(float2 a, float2 b)
{
    float ab_cosTheta = dot(a, b);
    float ab_pow2 = dot(a, a) * dot(b, b);
    return (ab_pow2 == .0) ? 1.
                           : clamp(ab_cosTheta * inversesqrt(ab_pow2), -1., 1.);
    // FIXME(crbug.com/800804,skbug.com/11268): This can overflow if we don't
    // normalize exponents.
}

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
INLINE void find_cubic_coeffs(float2 p0,
                              float2 p1,
                              float2 p2,
                              float2 p3,
                              OUT(float2) A,
                              OUT(float2) B,
                              OUT(float2) C)
{
    C = p1 - p0;
    float2 D = p2 - p1;
    float2 E = p3 - p0;
    B = D - C;
    A = -3. * D + E;
    // FIXME(crbug.com/800804,skbug.com/11268): Consider normalizing the
    // exponents in A,B,C at this point in order to prevent fp32 overflow.
}

// Tangent of the curve at T=0 and T=1.
INLINE float2x2 find_cubic_tangents(float2 p0, float2 p1, float2 p2, float2 p3)
{
    float2x2 t;
    t[0] = (any(notEqual(p0, p1)) ? p1 : any(notEqual(p1, p2)) ? p2 : p3) - p0;
    t[1] = p3 - (any(notEqual(p3, p2)) ? p2 : any(notEqual(p2, p1)) ? p1 : p0);
    return t;
}

// Measure the amount of curvature, in radians centered at location T, and
// covering a spread of width "desiredSpread" in local coordinates. If
// "desiredSpread" would reach outside the range T=0..1, the spread is clipped
// to stay within range.
INLINE float measure_cubic_local_curvature(float2 p0,
                                           float2 p1,
                                           float2 p2,
                                           float2 p3,
                                           float T,
                                           float desiredSpread)
{
    float2 A, B, C;
    find_cubic_coeffs(p0, p1, p2, p3, A, B, C);
    float2 tangent = 3. * (((A * T) + 2. * B) * T + C);
    float lengthTan = length(tangent);
    if (lengthTan == .0)
    {
        return .0;
    }

    // Define the function:
    //
    //    Spread(dt) = A_*dt^3 + C_*dt
    //
    // Which calculates the spread of the curve in local coordinates, parallel
    // to tangent, over the range "T - dt .. T + dt".
    tangent *= 1. / lengthTan;
    float A_ = 2. * dot(A, tangent);
    float C_ = 3. * (A_ * T + 4. * dot(B, tangent)) * T + 6. * dot(C, tangent);

    // Decide the "targetSpread" across which we will measure curvature. Ideally
    // this is "desiredSpread", but use less than that if that would reach
    // outside T=0..1.
    float maxDT = min(T, 1. - T);
    float maxSpread = (A_ * maxDT * maxDT + C_) * maxDT;
    // Pad the maxSpread to guarantee we won't step outside T=0..1.
    float targetSpread = min(desiredSpread, maxSpread * .9999);

    // Solve for dt, where Spread(dt) == targetSpread.
    float dt;
    if (A_ == .0)
    {
        // Degenerate case: Spread(dt) == C_*dt.
        dt = targetSpread / C_;
    }
    else
    {
        // Solve the normalized cubic x^3 + ax^2 + bx + c == 0.
        // (Numerical Recipes in C, 5.6 Quadratic and Cubic Equations,
        // https://hd.fizyka.umk.pl/~jacek/docs/nrc/c5-6.pdf)
        float r = 1. / A_;
        float /*a = 0,*/ b = C_ * r, c = -targetSpread * r;
        float Q = (-1. / 3.) * b, R = .5 * c;
        float discr = R * R - Q * Q * Q;
        if (discr < .0)
        {
            float sqrtQ = sqrt(Q);
            float theta = acos(R / (sqrtQ * sqrtQ * sqrtQ));
            // The 3 roots are: (because a == 0)
            //   -2 * sqrt(Q) * cos(theta/3 + float3{0, 1, -1} * 2*pi/3)
            // We want the root closest to zero, which will be the 3rd root
            // because its argument for cos() is always closest to +-pi/2.
            dt = -2. * sqrtQ * cos(theta * (1. / 3.) + (-PI * 2. / 3.));
        }
        else
        {
            float A = pow(abs(R) + sqrt(discr), 1. / 3.);
            if (R < .0)
                A = -A;
            dt = A != .0 ? A + Q / A : .0;
        }
    }
    dt = abs(dt);

    // Measure curvature over the spread T - dt .. T + dt.
    float4 t0011 = T + make_float4(-dt, -dt, dt, dt);
    float4 tanDirs = (A.xyxy * t0011 + 2. * B.xyxy) * t0011 + C.xyxy;
    // Use more stable tangents at the ends in case we've encountered a cusp.
    float2x2 tangents = find_cubic_tangents(p0, p1, p2, p3);
    float2 tan0 = t0011.x < 1e-3 ? tangents[0] : tanDirs.xy;
    float2 tan1 = t0011.z > 1. - 1e-3 ? tangents[1] : tanDirs.zw;
    // NOTE: this will break if there is an inflection point.
    return acos(cosine_between_vectors(tan0, tan1));
}

// Returns clamp(a/b, 0, 1).
// Returns 1 if a/b == INF.
// Returns 0 if a/b == -INF.
// Returns 0 if a == 0 and b == 0.
// Returns 0 or 1 if a or b are NaN.
// Returns 0 or 1 if a and b are both +/-INF.
INLINE float clamped_divide(float a, float b)
{
    a = b < .0 ? -a : a;
    b = abs(b);
    return a > .0 ? (a < b ? a / b : 1.) : .0;
}

// Find the location and value of a cubic's maximum height, relative to the
// baseline p0->p3.
float find_cubic_max_height(float2 p0,
                            float2 p1,
                            float2 p2,
                            float2 p3,
                            OUT(float) outT)
{
    // Find the cubic height function, which is also a 1D cubic bezier:
    //
    //   height - 3(dht^3 - (h1 + dh)t^2 + h1t)
    //
    float2 base = p3 - p0;
    float lengthBase = length(p3 - p0);
    if (lengthBase == .0)
    {
        outT = .5;
        return .0;
    }
    float2 norm = make_float2(-base.y, base.x) / lengthBase;
    float h2 = dot(norm, p2 - p0);
    float h1 = dot(norm, p1 - p0);
    float dh = h1 - h2;

#if 0
    // A cubic's height function has two maxima. Find both.
    float a = 3. * dh;
    float b_over_minus_2 = dh + h1;
    float c = h1;
    float q = sqrt(max(dh * dh + h2 * h1, .0));
    if (b_over_minus_2 < .0)
        q = -q;
    q += b_over_minus_2;
    float2 tt = make_float2(clamped_divide(q, a), clamped_divide(c, q));
    float2 hh = 3. * (tt * (tt * (tt * dh - (h1 + dh)) + h1));

    // Go with whichever maximum is larger.
    hh = abs(hh);
    outT = hh.x > hh.y ? tt.x : tt.y;
    return max(hh.x, hh.y);
#else
    // Solve for max height iteratively using Newton-Raphson because the above
    // solution fails sometimes on ARM Mali.
    //
    // This works as easily as it does because the curve is pre-chopped to be
    // convex, meaning there is only one maxima in 0..1.
    //
    // First find the power-basis coefficients for the height function:
    //
    //   height = 3(At^3 + Bt^2 + Ct)
    //
    float _3A = 3. * dh;
    float B = -h1 - dh;
    float C = h1;
    float t = .5; // First guess of max height is t=.5.
    // Per the unit test, 3 iters is enough to fall within 1e-3 of max height.
    for (int i = 0; i < 3; ++i)
    {
        // Standard Newton-Rhapson:
        //
        //   h'(t) = 3(3At^2 + 2Bt + C)
        //   h''(t) = 3(6At + 2B)
        //   t -= h'(t) / h''(t)
        //
        // This reduces to:
        //
        //   t = (3At^2 - C) / (6At + 2B)
        //
        float _3At = _3A * t;
        t = clamped_divide(_3At * t - C, 2. * (_3At + B));
    }
    outT = t;
    return abs(t * (t * (t * _3A + 3. * B) + 3. * C));
#endif
}
