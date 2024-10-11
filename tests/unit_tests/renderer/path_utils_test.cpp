/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "path_utils.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/simd.hpp"
#include <catch.hpp>

namespace rive
{
constexpr static float kEpsilon = 1.f / (1 << 12);

static bool fuzzy_equal(float a, float b, float tolerance = kEpsilon)
{
    assert(tolerance >= 0);
    return fabsf(a - b) <= tolerance;
}

static float frand() { return rand() / static_cast<float>(RAND_MAX); }

TEST_CASE("ChopCubicAt", "[pathutils]")
{
#if 0
    /*
        Inspired by this test, which used to assert that the tValues had dups

        <path stroke="#202020" d="M0,0 C0,0 1,1 2190,5130 C2190,5070 2220,5010 2205,4980" />
     */
    const Vec2D src[] = {
        {2190, 5130},
        {2190, 5070},
        {2220, 5010},
        {2205, 4980},
    };
    Vec2D dst[13];
    float tValues[3];
    // make sure we don't assert internally
    int count = SkChopCubicAtMaxCurvature(src, dst, tValues);
    if ((false))
    { // avoid bit rot, suppress warning
        CHECK(count);
    }
#endif
    // Make sure src and dst can be the same pointer.
    {
        Vec2D pts[7];
        for (int i = 0; i < 7; ++i)
        {
            pts[i] = {(float)i, (float)i};
        }
        pathutils::ChopCubicAt(pts, pts, .5f);
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
        pathutils::ChopCubicAt(pts, allChops, chopTs, std::size(chopTs));
        int i = 3;
        for (float chopT : chopTs)
        {
            // Ensure we chop at approximately the correct points when we chop
            // an entire list.
            Vec2D expectedPt = pathutils::EvalCubicAt(pts, chopT);
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
            pathutils::ChopCubicAt(pts, localChops, chopT, chopT);
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
        pathutils::ChopCubicAt(pts, oneChops, ones, std::size(ones));
        CHECK(oneChops[0] == pts[0]);
        CHECK(oneChops[1] == pts[1]);
        CHECK(oneChops[2] == pts[2]);
        for (size_t index = 3; index < std::size(oneChops); ++index)
        {
            CHECK(oneChops[index] == pts[3]);
        }
    }
}

TEST_CASE("ChopCubicAt(tValues=null)", "[pathutils]")
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
        pathutils::ChopCubicAt(pts, chopTChops, chopT, numChops);
        pathutils::ChopCubicAt(pts, nullTChops, nullptr, numChops);
        size_t numChoptPts = numChops * 3 + 1;
        for (size_t i = 0; i <= numChoptPts; ++i)
        {
            CHECK(nullTChops[i].x == Approx(chopTChops[i].x));
            CHECK(nullTChops[i].y == Approx(chopTChops[i].y));
        }
    }
}

static float SkMeasureNonInflectCubicRotation(const Vec2D pts[4])
{
    Vec2D a = pts[1] - pts[0];
    Vec2D b = pts[2] - pts[1];
    Vec2D c = pts[3] - pts[2];
    if (a == Vec2D())
    {
        return pathutils::MeasureAngleBetweenVectors(b, c);
    }
    if (b == Vec2D())
    {
        return pathutils::MeasureAngleBetweenVectors(a, c);
    }
    if (c == Vec2D())
    {
        return pathutils::MeasureAngleBetweenVectors(a, b);
    }
    // Postulate: When no points are colocated and there are no inflection
    // points in T=0..1, the rotation is: 360 degrees, minus the angle
    // [p0,p1,p2], minus the angle [p1,p2,p3].
    return 2 * math::PI - pathutils::MeasureAngleBetweenVectors(a, -b) -
           pathutils::MeasureAngleBetweenVectors(b, -c);
}

TEST_CASE("SkMeasureNonInflectCubicRotation", "[pathutils]")
{
    static Vec2D kFlatCubic[4] = {{0, 0}, {0, 1}, {0, 2}, {0, 3}};
    CHECK(fuzzy_equal(SkMeasureNonInflectCubicRotation(kFlatCubic), 0));

    static Vec2D kFlatCubic180_1[4] = {{0, 0}, {1, 0}, {3, 0}, {2, 0}};
    CHECK(fuzzy_equal(SkMeasureNonInflectCubicRotation(kFlatCubic180_1),
                      math::PI));

    static Vec2D kFlatCubic180_2[4] = {{0, 1}, {0, 0}, {0, 2}, {0, 3}};
    CHECK(fuzzy_equal(SkMeasureNonInflectCubicRotation(kFlatCubic180_2),
                      math::PI));

    static Vec2D kFlatCubic360[4] = {{0, 1}, {0, 0}, {0, 3}, {0, 2}};
    CHECK(fuzzy_equal(SkMeasureNonInflectCubicRotation(kFlatCubic360),
                      2 * math::PI));

    static Vec2D kSquare180[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
    CHECK(fuzzy_equal(SkMeasureNonInflectCubicRotation(kSquare180), math::PI));

    auto checkQuadRotation = [](const Vec2D pts[3], float expectedRotation) {
#if 0
        float r = SkMeasureQuadRotation(pts);
        CHECK(fuzzy_equal(r, expectedRotation));
#endif
        Vec2D cubic1[4] = {pts[0], pts[0], pts[1], pts[2]};
        CHECK(fuzzy_equal(SkMeasureNonInflectCubicRotation(cubic1),
                          expectedRotation));

        Vec2D cubic2[4] = {pts[0], pts[1], pts[1], pts[2]};
        CHECK(fuzzy_equal(SkMeasureNonInflectCubicRotation(cubic2),
                          expectedRotation));

        Vec2D cubic3[4] = {pts[0], pts[1], pts[2], pts[2]};
        CHECK(fuzzy_equal(SkMeasureNonInflectCubicRotation(cubic3),
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
            pathutils::FindCubicConvex180Chops(p, convex180T, &areCusps);
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
        float totalRotation = SkMeasureNonInflectCubicRotation(p);
        int convex180N =
            pathutils::FindCubicConvex180Chops(p, convex180T, &areCusps);
        Vec2D chops[10];
        pathutils::ChopCubicAt(p, chops, convex180T, convex180N);
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
                pathutils::ChopCubicAt(p,
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
            float rads = SkMeasureNonInflectCubicRotation(chops + i * 3);
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
                REQUIRE(fuzzy_equal(SkMeasureNonInflectCubicRotation(chops),
                                    math::PI));
                REQUIRE(fuzzy_equal(SkMeasureNonInflectCubicRotation(chops + 3),
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

TEST_CASE("FindCubicConvex180Chops", "[pathutils]")
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
    CHECK(pathutils::FindCubicConvex180Chops(quad, T, &areCusps) == 0);

    // Now test that cusps and near-cusps get flagged as cusps.
    Vec2D cusp[4] = {{0, 0}, {1, 1}, {1, 0}, {0, 1}};
    CHECK(pathutils::FindCubicConvex180Chops(cusp, T, &areCusps) == 1);
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
    CHECK(pathutils::FindCubicConvex180Chops(cusp, T, &areCusps) == 2);
    CHECK(areCusps == false);

    // Ensure two inflection points barely less than epsilon apart do get
    // flagged as cusps.
    cusp[1].y = (float)(1 - .9 * dy);
    cusp[2].y = (float)(0 + .9 * dy);
    CHECK(pathutils::FindCubicConvex180Chops(cusp, T, &areCusps) == 1);
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
    CHECK(pathutils::FindCubicConvex180Chops(cubic, T, &areCusps) == 0);
}
} // namespace rive
