/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "rive/math/math_types.hpp"
#include "rive/math/bezier_utils.hpp"
#include "rive/math/simd.hpp"
#include "common/rand.hpp"
#include <catch.hpp>

// glsl cross-compiling requires the clang/gcc vector extension.
#ifndef _MSC_VER
#include "../renderer/cpp.glsl"
namespace glsl_cross
{
#include "generated/shaders/bezier_utils.minified.glsl"
}
#endif

namespace rive
{
namespace math
{
constexpr static float kEpsilon = 1.f / (1 << 12);

static bool fuzzy_equal(float a, float b, float tolerance = kEpsilon)
{
    assert(tolerance >= 0);
    return fabsf(a - b) <= tolerance;
}

static float frand() { return rand() / static_cast<float>(RAND_MAX); }

TEST_CASE("chop_cubic_at", "[bezier_utils]")
{
    // Make sure src and dst can be the same pointer.
    {
        Vec2D pts[7];
        for (int i = 0; i < 7; ++i)
        {
            pts[i] = {(float)i, (float)i};
        }
        math::chop_cubic_at(pts, pts, .5f);
        for (int i = 0; i < 7; ++i)
        {
            CHECK(pts[i].x == pts[i].y);
            CHECK(pts[i].x == i * .5f);
        }
    }

    static const float chopTs[] = {
        0,        3 / 83.f, 3 / 79.f, 3 / 73.f, 3 / 71.f, 3 / 67.f,
        3 / 61.f, 3 / 59.f, 3 / 53.f, 3 / 47.f, 3 / 43.f, 3 / 41.f,
        3 / 37.f, 3 / 31.f, 3 / 29.f, 3 / 23.f, 3 / 19.f, 3 / 17.f,
        3 / 13.f, 3 / 11.f, 3 / 7.f,  3 / 5.f,  1,
    };
    float ones[] = {1, 1, 1, 1, 1};

    // Ensure an odd number of T values so we exercise the single chop code at
    // the end of SkChopCubicAt form multiple T.
    static_assert(std::size(chopTs) % 2 == 1);
    static_assert(std::size(ones) % 2 == 1);

    for (int iterIdx = 0; iterIdx < 5; ++iterIdx)
    {
        Vec2D pts[4] = {{frand(), frand()},
                        {frand(), frand()},
                        {frand(), frand()},
                        {frand(), frand()}};

        Vec2D allChops[4 + std::size(chopTs) * 3];
        math::chop_cubic_at(pts, allChops, chopTs, std::size(chopTs));
        int i = 3;
        for (float chopT : chopTs)
        {
            // Ensure we chop at approximately the correct points when we chop
            // an entire list.
            Vec2D expectedPt = math::eval_cubic_at(pts, chopT);
            CHECK(fuzzy_equal(allChops[i].x, expectedPt.x));
            CHECK(fuzzy_equal(allChops[i].y, expectedPt.y));
            if (chopT == 0)
            {
                CHECK(allChops[i] == pts[0]);
            }
            if (chopT == 1)
            {
                CHECK(allChops[i] == pts[3]);
            }
            i += 3;

            // Ensure the middle is exactly degenerate when we chop at two equal
            // points.
            Vec2D localChops[10];
            math::chop_cubic_at(pts, localChops, chopT, chopT);
            CHECK(localChops[3] == localChops[4]);
            CHECK(localChops[3] == localChops[5]);
            CHECK(localChops[3] == localChops[6]);
            if (chopT == 0)
            {
                // Also ensure the first curve is exactly p0 when we chop at
                // T=0.
                CHECK(localChops[0] == pts[0]);
                CHECK(localChops[1] == pts[0]);
                CHECK(localChops[2] == pts[0]);
                CHECK(localChops[3] == pts[0]);
            }
            if (chopT == 1)
            {
                // Also ensure the last curve is exactly p3 when we chop at T=1.
                CHECK(localChops[6] == pts[3]);
                CHECK(localChops[7] == pts[3]);
                CHECK(localChops[8] == pts[3]);
                CHECK(localChops[9] == pts[3]);
            }
        }

        // Now test what happens when SkChopCubicAt does 0/0 and gets NaN
        // values.
        Vec2D oneChops[4 + std::size(ones) * 3];
        math::chop_cubic_at(pts, oneChops, ones, std::size(ones));
        CHECK(oneChops[0] == pts[0]);
        CHECK(oneChops[1] == pts[1]);
        CHECK(oneChops[2] == pts[2]);
        for (size_t index = 3; index < std::size(oneChops); ++index)
        {
            CHECK(oneChops[index] == pts[3]);
        }
    }
}

TEST_CASE("chop_cubic_at(tValues=null)", "[bezier_utils]")
{
    constexpr int kMaxNumChops = 20;
    float chopT[kMaxNumChops];
    Vec2D chopTChops[(kMaxNumChops + 1) * 3 + 1];
    Vec2D nullTChops[(kMaxNumChops + 1) * 3 + 1];
    for (int numChops = 1; numChops <= kMaxNumChops; ++numChops)
    {
        Vec2D pts[4] = {{frand(), frand()},
                        {frand(), frand()},
                        {frand(), frand()},
                        {frand(), frand()}};
        float stepT = 1.f / (numChops + 1);
        float t = 0;
        for (size_t i = 0; i < numChops; ++i)
        {
            chopT[i] = t += stepT;
        }
        math::chop_cubic_at(pts, chopTChops, chopT, numChops);
        math::chop_cubic_at(pts, nullTChops, nullptr, numChops);
        size_t numChoptPts = numChops * 3 + 1;
        for (size_t i = 0; i <= numChoptPts; ++i)
        {
            CHECK(nullTChops[i].x == Approx(chopTChops[i].x));
            CHECK(nullTChops[i].y == Approx(chopTChops[i].y));
        }
    }
}

static float measure_non_inflect_cubic_rotation(const Vec2D pts[4])
{
    Vec2D a = pts[1] - pts[0];
    Vec2D b = pts[2] - pts[1];
    Vec2D c = pts[3] - pts[2];
    float lengthA = a.length();
    float lengthB = b.length();
    float lengthC = c.length();
    float lengthMax = std::max(std::max(lengthA, lengthB), lengthC);
    float zeroThreshold = std::max(lengthMax, 1.f) * 1e-4f;
    if (lengthA <= zeroThreshold)
    {
        return math::measure_angle_between_vectors(b, c);
    }
    if (lengthB <= zeroThreshold)
    {
        return math::measure_angle_between_vectors(a, c);
    }
    if (lengthC <= zeroThreshold)
    {
        return math::measure_angle_between_vectors(a, b);
    }
    // Postulate: When no points are colocated and there are no inflection
    // points in T=0..1, the rotation is: 360 degrees, minus the angle
    // [p0,p1,p2], minus the angle [p1,p2,p3].
    return 2 * math::PI - math::measure_angle_between_vectors(a, -b) -
           math::measure_angle_between_vectors(b, -c);
}

TEST_CASE("measure_non_inflect_cubic_rotation", "[bezier_utils]")
{
    static Vec2D kFlatCubic[4] = {{0, 0}, {0, 1}, {0, 2}, {0, 3}};
    CHECK(fuzzy_equal(measure_non_inflect_cubic_rotation(kFlatCubic), 0));

    static Vec2D kFlatCubic180_1[4] = {{0, 0}, {1, 0}, {3, 0}, {2, 0}};
    CHECK(fuzzy_equal(measure_non_inflect_cubic_rotation(kFlatCubic180_1),
                      math::PI));

    static Vec2D kFlatCubic180_2[4] = {{0, 1}, {0, 0}, {0, 2}, {0, 3}};
    CHECK(fuzzy_equal(measure_non_inflect_cubic_rotation(kFlatCubic180_2),
                      math::PI));

    static Vec2D kFlatCubic360[4] = {{0, 1}, {0, 0}, {0, 3}, {0, 2}};
    CHECK(fuzzy_equal(measure_non_inflect_cubic_rotation(kFlatCubic360),
                      2 * math::PI));

    static Vec2D kSquare180[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
    CHECK(
        fuzzy_equal(measure_non_inflect_cubic_rotation(kSquare180), math::PI));

    auto checkQuadRotation = [](const Vec2D pts[3], float expectedRotation) {
#if 0
        float r = SkMeasureQuadRotation(pts);
        CHECK(fuzzy_equal(r, expectedRotation));
#endif
        Vec2D cubic1[4] = {pts[0], pts[0], pts[1], pts[2]};
        CHECK(fuzzy_equal(measure_non_inflect_cubic_rotation(cubic1),
                          expectedRotation));

        Vec2D cubic2[4] = {pts[0], pts[1], pts[1], pts[2]};
        CHECK(fuzzy_equal(measure_non_inflect_cubic_rotation(cubic2),
                          expectedRotation));

        Vec2D cubic3[4] = {pts[0], pts[1], pts[2], pts[2]};
        CHECK(fuzzy_equal(measure_non_inflect_cubic_rotation(cubic3),
                          expectedRotation));
    };

    static Vec2D kFlatQuad[4] = {{0, 0}, {0, 1}, {0, 2}};
    checkQuadRotation(kFlatQuad, 0);

    static Vec2D kFlatQuad180_1[4] = {{1, 0}, {0, 0}, {2, 0}};
    checkQuadRotation(kFlatQuad180_1, math::PI);

    static Vec2D kFlatQuad180_2[4] = {{0, 0}, {0, 2}, {0, 1}};
    checkQuadRotation(kFlatQuad180_2, math::PI);

    static Vec2D kTri120[3] = {{0, 0}, {.5f, std::sqrt(3.f) / 2}, {1, 0}};
    checkQuadRotation(kTri120, 2 * math::PI / 3);
}

static bool is_linear(Vec2D p0, Vec2D p1, Vec2D p2)
{
    return fuzzy_equal(Vec2D::cross(p0 - p1, p2 - p1), 0);
}

static bool is_linear(const Vec2D p[4])
{
    return is_linear(p[0], p[1], p[2]) && is_linear(p[0], p[2], p[3]) &&
           is_linear(p[1], p[2], p[3]);
}

static int valid_unit_divide(float numer, float denom, float* ratio)
{
    assert(ratio);

    if (numer < 0)
    {
        numer = -numer;
        denom = -denom;
    }

    if (denom == 0 || numer == 0 || numer >= denom)
    {
        return 0;
    }

    float r = numer / denom;
    if (std::isnan(r))
    {
        return 0;
    }
    assert(r >= 0 && r < 1.f);
    if (r == 0)
    { // catch underflow if numer <<<< denom
        return 0;
    }
    *ratio = r;
    return 1;
}

// Just returns its argument, but makes it easy to set a break-point to know
// when SkFindUnitQuadRoots is going to return 0 (an error).
static int return_check_zero(int value)
{
    if (value == 0)
    {
        return 0;
    }
    return value;
}

static int SkFindUnitQuadRoots(float A, float B, float C, float roots[2])
{
    assert(roots);

    if (A == 0)
    {
        return return_check_zero(valid_unit_divide(-C, B, roots));
    }

    float* r = roots;

    // use doubles so we don't overflow temporarily trying to compute R
    double dr = (double)B * B - 4 * (double)A * C;
    if (dr < 0)
    {
        return return_check_zero(0);
    }
    dr = sqrt(dr);
    float R = static_cast<float>(dr);
    if (std::isnan(R) || std::isinf(R))
    {
        return return_check_zero(0);
    }

    float Q = (B < 0) ? -(B - R) / 2 : -(B + R) / 2;
    r += valid_unit_divide(Q, A, r);
    r += valid_unit_divide(C, Q, r);
    if (r - roots == 2)
    {
        if (roots[0] > roots[1])
        {
            using std::swap;
            swap(roots[0], roots[1]);
        }
        else if (roots[0] == roots[1])
        {           // nearly-equal?
            r -= 1; // skip the double root
        }
    }
    return return_check_zero((int)(r - roots));
}

/** http://www.faculty.idc.ac.il/arik/quality/appendixA.html

    Inflection means that curvature is zero.
    Curvature is [F' x F''] / [F'^3]
    So we solve F'x X F''y - F'y X F''y == 0
    After some canceling of the cubic term, we get
    A = b - a
    B = c - 2b + a
    C = d - 3c + 3b - a
    (BxCy - ByCx)t^2 + (AxCy - AyCx)t + AxBy - AyBx == 0
*/
static int SkFindCubicInflections(const Vec2D src[4], float tValues[2])
{
    float Ax = src[1].x - src[0].x;
    float Ay = src[1].y - src[0].y;
    float Bx = src[2].x - 2 * src[1].x + src[0].x;
    float By = src[2].y - 2 * src[1].y + src[0].y;
    float Cx = src[3].x + 3 * (src[1].x - src[2].x) - src[0].x;
    float Cy = src[3].y + 3 * (src[1].y - src[2].y) - src[0].y;

    return SkFindUnitQuadRoots(Bx * Cy - By * Cx,
                               Ax * Cy - Ay * Cx,
                               Ax * By - Ay * Bx,
                               tValues);
}

static void check_cubic_convex_180(const Vec2D p[4])
{
    bool areCusps = false;
    float inflectT[2], convex180T[2];
    if (int inflectN = SkFindCubicInflections(p, inflectT))
    {
        // The curve has inflections. FindCubicConvex180Chops should return the
        // inflection points.
        int convex180N =
            math::find_cubic_convex_180_chops(p, convex180T, &areCusps);
        REQUIRE(inflectN == convex180N);
        if (!areCusps)
        {
            REQUIRE((inflectN == 1 ||
                     fabsf(inflectT[0] - inflectT[1]) >= kEpsilon));
        }
        for (int i = 0; i < convex180N; ++i)
        {
            REQUIRE(fuzzy_equal(inflectT[i], convex180T[i]));
        }
    }
    else
    {
        float totalRotation = measure_non_inflect_cubic_rotation(p);
        int convex180N =
            math::find_cubic_convex_180_chops(p, convex180T, &areCusps);
        Vec2D chops[10];
        math::chop_cubic_at(p, chops, convex180T, convex180N);
        float radsSum = 0;
        if (areCusps)
        {
            if (convex180N == 1)
            {
                // The cusp itself accounts for 180 degrees of rotation.
                radsSum = math::PI;
                // Rechop, this time straddling the cusp, to avoid fp32
                // precision issues from crossover.
                Vec2D straddleChops[10];
                math::chop_cubic_at(p,
                                    straddleChops,
                                    std::max(convex180T[0] - math::PI, 0.f),
                                    std::min(convex180T[0] + math::PI, 1.f));
                memcpy(chops + 1, straddleChops + 1, 2 * sizeof(Vec2D));
                memcpy(chops + 4, straddleChops + 7, 2 * sizeof(Vec2D));
            }
            else if (convex180N == 2)
            {
                // The only way a cubic can have two cusps is if it's flat with
                // two 180 degree turnarounds.
                radsSum = math::PI * 2;
                chops[1] = chops[0];
                chops[2] = chops[4] = chops[3];
                chops[5] = chops[7] = chops[6];
                chops[8] = chops[9];
            }
        }
        for (int i = 0; i <= convex180N; ++i)
        {
            float rads = measure_non_inflect_cubic_rotation(chops + i * 3);
            assert(rads < math::PI + kEpsilon);
            radsSum += rads;
        }
        if (totalRotation < math::PI - kEpsilon)
        {
            // The curve should never chop if rotation is <180 degrees.
            REQUIRE(convex180N == 0);
        }
        else if (!is_linear(p))
        {
            REQUIRE(fuzzy_equal(radsSum, totalRotation));
            if (totalRotation > math::PI + kEpsilon)
            {
                REQUIRE(convex180N == 1);
                // This works because cusps take the "inflection" path above, so
                // we don't get non-lilnear curves that lose rotation when
                // chopped.
                REQUIRE(fuzzy_equal(measure_non_inflect_cubic_rotation(chops),
                                    math::PI));
                REQUIRE(
                    fuzzy_equal(measure_non_inflect_cubic_rotation(chops + 3),
                                totalRotation - math::PI));
            }
            REQUIRE(!areCusps);
        }
        else
        {
            REQUIRE(areCusps);
        }
    }
}

TEST_CASE("find_cubic_convex_180_chops", "[bezier_utils]")
{
    // Test all combinations of corners from the square [0,0,1,1]. This covers
    // every cubic type as well as a wide variety of special cases for cusps,
    // lines, loops, and inflections.
    for (int i = 0; i < (1 << 8); ++i)
    {
        Vec2D p[4] = {Vec2D((i >> 0) & 1, (i >> 1) & 1),
                      Vec2D((i >> 2) & 1, (i >> 3) & 1),
                      Vec2D((i >> 4) & 1, (i >> 5) & 1),
                      Vec2D((i >> 6) & 1, (i >> 7) & 1)};
        check_cubic_convex_180(p);
    }

    {
        // This cubic has a convex-180 chop at T=1-"epsilon"
        static const uint32_t hexPts[] = {0x3ee0ac74,
                                          0x3f1e061a,
                                          0x3e0fc408,
                                          0x3f457230,
                                          0x3f42ac7c,
                                          0x3f70d76c,
                                          0x3f4e6520,
                                          0x3f6acafa};
        Vec2D p[4];
        memcpy(p, hexPts, sizeof(p));
        check_cubic_convex_180(p);
    }

    // Now test an exact quadratic.
    Vec2D quad[4] = {{0, 0}, {2, 2}, {4, 2}, {6, 0}};
    float T[2];
    bool areCusps;
    CHECK(math::find_cubic_convex_180_chops(quad, T, &areCusps) == 0);

    // Now test that cusps and near-cusps get flagged as cusps.
    Vec2D cusp[4] = {{0, 0}, {1, 1}, {1, 0}, {0, 1}};
    CHECK(math::find_cubic_convex_180_chops(cusp, T, &areCusps) == 1);
    CHECK(areCusps == true);

    // Find the height of the right side of "cusp" at which the distance between
    // its inflection points is epsilon (in parametric space).
    constexpr double epsilon = 1.0 / (1 << 11);
    constexpr double epsilonPow2 = epsilon * epsilon;
    double h = (1 - epsilonPow2) / (3 * epsilonPow2 + 1);
    double dy = (1 - h) / 2;
    cusp[1].y = (float)(1 - dy);
    cusp[2].y = (float)(0 + dy);
    CHECK(SkFindCubicInflections(cusp, T) == 2);
    CHECK(fuzzy_equal(T[1] - T[0], (float)epsilon, (float)epsilonPow2));

    // Ensure two inflection points barely more than epsilon apart do not get
    // flagged as cusps.
    cusp[1].y = (float)(1 - 4 * dy);
    cusp[2].y = (float)(0 + 4 * dy);
    CHECK(math::find_cubic_convex_180_chops(cusp, T, &areCusps) == 2);
    CHECK(areCusps == false);

    // Ensure two inflection points barely less than epsilon apart do get
    // flagged as cusps.
    cusp[1].y = (float)(1 - .9 * dy);
    cusp[2].y = (float)(0 + .9 * dy);
    CHECK(math::find_cubic_convex_180_chops(cusp, T, &areCusps) == 1);
    CHECK(areCusps == true);

    // Ensure fast floatingpoint is not enbled.
    float2 p[] = {{460, 1060},
                  {774, 526},
                  {60, 660},
                  {460, 460},
                  {667, 460},
                  {1060, 460},
                  {686, 460},
                  {686, 660},
                  {1042, 1020}};
    float2 c0 = simd::mix(p[3], p[4], float2(2 / 3.f));
    float2 c1 = simd::mix(p[5], p[4], float2(2 / 3.f));
    Vec2D cubic[] = {math::bit_cast<Vec2D>(p[3]),
                     math::bit_cast<Vec2D>(c0),
                     math::bit_cast<Vec2D>(c1),
                     math::bit_cast<Vec2D>(p[5])};
    CHECK(math::find_cubic_convex_180_chops(cubic, T, &areCusps) == 0);
}

// Check that flat lines with ordered points never get detected as cusps.
TEST_CASE("find_cubic_convex_180_chops_lines", "[bezier_utils]")
{
    Vec2D p0 = {123, 200}, p3 = {223, 432};
    for (float t0 = 1e-3f; t0 < 1; t0 += .12f)
    {
        for (float t1 = t0 + .097f; t1 < 1; t1 += .097f)
        {
            Vec2D line[4] = {p0, lerp(p0, p3, t0), lerp(p0, p3, t1), p3};
            float _[2];
            bool areCusps = true;
            math::find_cubic_convex_180_chops(line, _, &areCusps);
            CHECK_FALSE(areCusps);
        }
    }
}

Vec2D eval_cubic(const Vec2D* p, float t)
{
    return math::pow3(1 - t) * p[0] + 3 * math::pow2(1 - t) * t * p[1] +
           3 * (1 - t) * math::pow2(t) * p[2] + math::pow3(t) * p[3];
}

constexpr static Vec2D TEST_CUBICS[][4] = {
    {{199, 1225}, {197, 943}, {349, 607}, {549, 427}},
    {{549, 427}, {349, 607}, {197, 943}, {199, 1225}},
    {{460, 1060}, {403, -320}, {60, 660}, {1181, 634}},
    {{1181, 634}, {60, 660}, {403, -320}, {460, 1060}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {100, 100}},
    {{0, 0}, {0, 0}, {100, 100}, {100, 100}},
    {{0, 0}, {100, 100}, {100, 100}, {0, 0}},
    {{-100, -100}, {100, 100}, {100, -100}, {-100, 100}}, // Cusp
    {{0, 0}, {0, 0}, {100, 100}, {100, 100}},             // Line
    {{0, 0}, {-100, -100}, {200, 200}, {100, 100}},       // Line w/ 2 cusps
    {{0, 0},
     {50 * 2.f / 3.f, 100 * 2.f / 3.f},
     {100 - 50 * 2.f / 3.f, 100 * 2.f / 3.f},
     {100, 0}}, // Quadratic
    // The remaining cubics had some sort of issue during development. They're
    // in the list to make sure they don't regress.
    {{0, 0},
     {50 * 2.f / 3.f, 100 * 2.f / 3.f},
     {100 - 50 * 2.f / 3.f, 100 * 2.f / 3.f},
     {100, 100}},
    {{100, 0}, {0, 0}, {0, 0}, {0, 0}},
};

static void check_eval_cubic(const EvalCubic& evalCubic,
                             const Vec2D* cubic,
                             float t0,
                             float t1)
{
    float4 pp = evalCubic(float4{t0, t0, t1, t1});
    Vec2D p0ref = eval_cubic(cubic, t0);
    Vec2D p1ref = eval_cubic(cubic, t1);
    CHECK(pp.x == Approx(p0ref.x).margin(1e-3f));
    CHECK(pp.y == Approx(p0ref.y).margin(1e-3f));
    CHECK(pp.z == Approx(p1ref.x).margin(1e-3f));
    CHECK(pp.w == Approx(p1ref.y).margin(1e-3f));
    float2 p0 = evalCubic(t0);
    float2 p1 = evalCubic(t1);
    CHECK(p0.x == Approx(p0ref.x).margin(1e-3f));
    CHECK(p0.y == Approx(p0ref.y).margin(1e-3f));
    CHECK(p1.x == Approx(p1ref.x).margin(1e-3f));
    CHECK(p1.y == Approx(p1ref.y).margin(1e-3f));
}

TEST_CASE("EvalCubic", "[bezier_utils]")
{
    for (auto cubic : TEST_CUBICS)
    {
        math::EvalCubic evalCubic(cubic);
        check_eval_cubic(evalCubic, cubic, 0, 1);
        for (float t = 0; t <= 1; t += 1e-2f)
        {
            check_eval_cubic(evalCubic, cubic, t, t + 3e-3f);
        }
    }
}

// glsl cross-compiling requires the clang/gcc vector extension.
#ifndef _MSC_VER
static void check_cubic_coeffs_tangents_glsl(const Vec2D* pts)
{
    // Check cubic coefficients.
    math::CubicCoeffs coeffs(pts);
    float2 p0 = simd::load2f(pts + 0);
    float2 p1 = simd::load2f(pts + 1);
    float2 p2 = simd::load2f(pts + 2);
    float2 p3 = simd::load2f(pts + 3);
    float2 A, B, C;
    glsl_cross::find_cubic_coeffs(p0, p1, p2, p3, A, B, C);
    CHECK(simd::all(A == coeffs.A));
    CHECK(simd::all(B == coeffs.B));
    CHECK(simd::all(C == coeffs.C));

    // Check cubic tangents.
    Vec2D tangents[2];
    math::find_cubic_tangents(pts, tangents);
    std::array<float2, 2> glslTangents =
        glsl_cross::find_cubic_tangents(p0, p1, p2, p3);
    CHECK(simd::all(simd::load2f(&tangents[0]) == glslTangents[0]));
    CHECK(simd::all(simd::load2f(&tangents[1]) == glslTangents[1]));
}

TEST_CASE("find_cubic_coeffs_tangents_glsl", "[bezier_utils]")
{
    for (auto pts : TEST_CUBICS)
    {
        check_cubic_coeffs_tangents_glsl(pts);
    }
    // Test all combinations of corners from the square [0,0,1,1]. This covers
    // every cubic type as well as a wide variety of special cases for cusps,
    // lines, loops, and inflections.
    for (int i = 0; i < (1 << 8); ++i)
    {
        Vec2D pts[4] = {Vec2D((i >> 0) & 1, (i >> 1) & 1) * 100,
                        Vec2D((i >> 2) & 1, (i >> 3) & 1) * 100,
                        Vec2D((i >> 4) & 1, (i >> 5) & 1) * 100,
                        Vec2D((i >> 6) & 1, (i >> 7) & 1) * 100};
        check_cubic_coeffs_tangents_glsl(pts);
    }
    Rand rando;
    rando.seed(0);
    for (int i = 0; i < 100; ++i)
    {
        Vec2D randos[] = {
            {rando.f32(-100, 100), rando.f32(-100, 100)},
            {rando.f32(-100, 100), rando.f32(-100, 100)},
            {rando.f32(-100, 100), rando.f32(-100, 100)},
            {rando.f32(-100, 100), rando.f32(-100, 100)},
        };
        check_cubic_coeffs_tangents_glsl(randos);
    }
}

TEST_CASE("clamped_divide_glsl", "[bezier_utils]")
{
    constexpr static float INF = std::numeric_limits<float>::infinity();
    CHECK(glsl_cross::clamped_divide(1, 2) == .5f);
    CHECK(glsl_cross::clamped_divide(-2, 0) == 0);
    CHECK(glsl_cross::clamped_divide(1, 0) == 1);
    CHECK(glsl_cross::clamped_divide(-INF, 1) == 0);
    CHECK(glsl_cross::clamped_divide(INF, 1) == 1);
    CHECK(glsl_cross::clamped_divide(INF, INF) == 1);
    CHECK(glsl_cross::clamped_divide(-INF, -INF) == 1);
    CHECK(glsl_cross::clamped_divide(-INF, INF) == 0);
    CHECK(glsl_cross::clamped_divide(INF, -INF) == 0);
    CHECK(glsl_cross::clamped_divide(0, 0) == 0);
    CHECK(glsl_cross::clamped_divide(1, NAN) == 1);
    CHECK(glsl_cross::clamped_divide(-1, NAN) == 0);
    CHECK(glsl_cross::clamped_divide(NAN, 1) == 0);
    CHECK(glsl_cross::clamped_divide(NAN, -1) == 0);
    CHECK(glsl_cross::clamped_divide(NAN, NAN) == 0);
}

static void check_cubic_max_height_glsl(const Vec2D* pts)
{
    float t, h = glsl_cross::find_cubic_max_height(simd::load2f(pts + 0),
                                                   simd::load2f(pts + 1),
                                                   simd::load2f(pts + 2),
                                                   simd::load2f(pts + 3),
                                                   t);
    CHECK(h >= 0);
    CHECK(t >= 0);
    CHECK(t <= 1);
    Vec2D norm = (pts[3] - pts[0]).normalized();
    norm = {-norm.y, norm.x};
    float k = -Vec2D::dot(norm, pts[0]);
    auto heightAt = [=](float t) {
        return fabsf(Vec2D::dot(norm, math::eval_cubic_at(pts, t)) + k);
    };
    CHECK(heightAt(t) == Approx(h).margin(1e-4f));
    float epsilon = std::max(h, 1.f) * 1e-3f;
    CHECK(h + epsilon > heightAt(0));
    CHECK(h + epsilon > heightAt(1));
    for (float t2 = 0; t2 <= 1; t2 += .005137f)
    {
        CHECK(h + epsilon > heightAt(t2));
    }
}

TEST_CASE("find_cubic_max_height_glsl", "[bezier_utils]")
{
    for (auto pts : TEST_CUBICS)
    {
        check_cubic_max_height_glsl(pts);
    }
    // Test all combinations of corners from the square [0,0,1,1]. This covers
    // every cubic type as well as a wide variety of special cases for cusps,
    // lines, loops, and inflections.
    for (int i = 0; i < (1 << 8); ++i)
    {
        Vec2D pts[4] = {Vec2D((i >> 0) & 1, (i >> 1) & 1) * 100,
                        Vec2D((i >> 2) & 1, (i >> 3) & 1) * 100,
                        Vec2D((i >> 4) & 1, (i >> 5) & 1) * 100,
                        Vec2D((i >> 6) & 1, (i >> 7) & 1) * 100};
        check_cubic_max_height_glsl(pts);
    }
    Rand rando;
    rando.seed(0);
    for (int i = 0; i < 100; ++i)
    {
        Vec2D randos[] = {
            {rando.f32(-100, 100), rando.f32(-100, 100)},
            {rando.f32(-100, 100), rando.f32(-100, 100)},
            {rando.f32(-100, 100), rando.f32(-100, 100)},
            {rando.f32(-100, 100), rando.f32(-100, 100)},
        };
        check_cubic_max_height_glsl(randos);
    }
}

static float guess_cubic_local_curvature(const Vec2D* p,
                                         const CubicCoeffs& coeffs,
                                         float t,
                                         float desiredSpread)
{
    // Iteratively guess a spread and compare with the computed result.
    float maxDT = fminf(t, 1 - t) * .9999f;
    float2 tan = 3.f * ((coeffs.A * t + 2.f * coeffs.B) * t + coeffs.C);
    float lengthTan = sqrtf(simd::dot(tan, tan));
    tan /= lengthTan;
    float dt = 0;
    math::EvalCubic evalCubic(coeffs, p[0]);
    // This would converge faster if we used the derivative of the Spread(dt)
    // function from measure_cubic_local_curvature, but since the whole point of
    // this test is to validate that function, use a binary search instead.
    for (int i = 0; i < 24; ++i)
    {
        float guessDT = (dt + maxDT) * .5f;
        float guessT0 = t - guessDT;
        float guessT1 = t + guessDT;
        float4 endpts = evalCubic(float4{guessT0, guessT0, guessT1, guessT1});
        float spread = fabsf(simd::dot(endpts.zw - endpts.xy, tan));
        if (spread <= desiredSpread)
            dt = guessDT;
        else
            maxDT = guessDT;
    }
    Vec2D chops[10];
    math::chop_cubic_at(p, chops, t - dt, t + dt);
    return measure_non_inflect_cubic_rotation(chops + 3);
}

constexpr static float FEATHERING_CUSP_PADDING = 1e-3f;

static int find_cubic_convex_90_chops(const Vec2D pts[],
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
    constexpr static float TESS_EPSILON = 1.f / (1 << 10);
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

static void check_cubic_convex_90_chops(const Vec2D* pts)
{
    float T[4];
    bool areCusps;
    int n =
        find_cubic_convex_90_chops(pts, T, FEATHERING_CUSP_PADDING, &areCusps);
    CHECK(n <= 4);

    Vec2D chops[16];
    assert(n * 3 + 1 <= std::size(chops));
    math::chop_cubic_at(pts, chops, T, n);
    Vec2D* p = chops;
    for (int i = 0; i <= n; ++i, p += 3)
    {
        if (areCusps && (i & 1))
        {
            // If the chops are around a cusp, odd-numbered chops are a padding
            // section that passes through a cusp.
            continue;
        }
        CHECK(measure_non_inflect_cubic_rotation(p) <=
              Approx(math::PI / 2).margin(1e-2f));
    }
}

TEST_CASE("find_cubic_convex_90_chops", "[bezier_utils]")
{
    for (auto pts : TEST_CUBICS)
    {
        check_cubic_convex_90_chops(pts);
    }
    // Test all combinations of corners from the square [0,0,1,1]. This covers
    // every cubic type as well as a wide variety of special cases for cusps,
    // lines, loops, and inflections.
    for (int i = 0; i < (1 << 8); ++i)
    {
        Vec2D pts[4] = {Vec2D((i >> 0) & 1, (i >> 1) & 1) * 100,
                        Vec2D((i >> 2) & 1, (i >> 3) & 1) * 100,
                        Vec2D((i >> 4) & 1, (i >> 5) & 1) * 100,
                        Vec2D((i >> 6) & 1, (i >> 7) & 1) * 100};
        check_cubic_convex_90_chops(pts);
    }
    Rand rando;
    rando.seed(0);
    for (int i = 0; i < 100; ++i)
    {
        Vec2D randos[] = {
            {rando.f32(-100, 100), rando.f32(-100, 100)},
            {rando.f32(-100, 100), rando.f32(-100, 100)},
            {rando.f32(-100, 100), rando.f32(-100, 100)},
            {rando.f32(-100, 100), rando.f32(-100, 100)},
        };
        check_cubic_convex_90_chops(randos);
    }
}

static void check_cubic_local_curvature_glsl(const Vec2D* pts)
{
    float T[4];
    bool areCusps;
    int n =
        find_cubic_convex_90_chops(pts, T, FEATHERING_CUSP_PADDING, &areCusps);
    CHECK(n <= 4);

    Vec2D chops[16];
    assert(n * 3 + 1 <= std::size(chops));
    math::chop_cubic_at(pts, chops, T, n);
    Vec2D* p = chops;
    for (int i = 0; i <= n; ++i, p += 3)
    {
        if (areCusps && (i & 1))
        {
            // If the chops are around a cusp, odd-numbered chops are a padding
            // section that passes through a cusp.
            continue;
        }

        float2 p0 = simd::load2f(p + 0);
        float2 p1 = simd::load2f(p + 1);
        float2 p2 = simd::load2f(p + 2);
        float2 p3 = simd::load2f(p + 3);

        float maxHeightT;
        CHECK(measure_non_inflect_cubic_rotation(p) <=
              Approx(math::PI / 2).margin(2.6e-3f));
        glsl_cross::find_cubic_max_height(p0, p1, p2, p3, maxHeightT);
        CubicCoeffs coeffs(p);
        for (float desiredSpread : {1.f, 10.f, 100.f})
        {
            for (float t : {maxHeightT, .5f})
            {
                float theta =
                    glsl_cross::measure_cubic_local_curvature(p0,
                                                              p1,
                                                              p2,
                                                              p3,
                                                              t,
                                                              desiredSpread);
                CHECK(theta >= 0);
                CHECK(theta <= math::PI);

                float guess =
                    guess_cubic_local_curvature(p, coeffs, t, desiredSpread);
                CHECK(theta == Approx(guess).margin(1e-2f));
            }
        }
    }
}

TEST_CASE("measure_cubic_local_curvature_glsl", "[bezier_utils]")
{
    for (auto pts : TEST_CUBICS)
    {
        check_cubic_local_curvature_glsl(pts);
    }
    // Test all combinations of corners from the square [0,0,1,1]. This covers
    // every cubic type as well as a wide variety of special cases for cusps,
    // lines, loops, and inflections.
    for (int i = 0; i < (1 << 8); ++i)
    {
        Vec2D pts[4] = {Vec2D((i >> 0) & 1, (i >> 1) & 1) * 100,
                        Vec2D((i >> 2) & 1, (i >> 3) & 1) * 100,
                        Vec2D((i >> 4) & 1, (i >> 5) & 1) * 100,
                        Vec2D((i >> 6) & 1, (i >> 7) & 1) * 100};
        check_cubic_local_curvature_glsl(pts);
    }
    Rand rando;
    rando.seed(0);
    for (int i = 0; i < 100; ++i)
    {
        Vec2D randos[] = {
            {rando.f32(-100, 100), rando.f32(-100, 100)},
            {rando.f32(-100, 100), rando.f32(-100, 100)},
            {rando.f32(-100, 100), rando.f32(-100, 100)},
            {rando.f32(-100, 100), rando.f32(-100, 100)},
        };
        check_cubic_local_curvature_glsl(randos);
    }
}
#endif
} // namespace math
} // namespace rive
