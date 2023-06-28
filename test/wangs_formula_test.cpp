/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Initial import from skia:tests/WangsFormulaTest.cpp
 *
 * Copyright 2023 Rive
 */

#include "rive/math/wangs_formula.hpp"
#include <catch.hpp>
#include <functional>

namespace rive
{
constexpr static float kPrecision = 4;
constexpr static float kEpsilon = 1.f / (1 << 12);

static bool fuzzy_equal(float a, float b, float tolerance = kEpsilon)
{
    assert(tolerance >= 0);
    return fabsf(a - b) <= tolerance;
}

const Vec2D kSerp[4] = {{285.625f, 499.687f},
                        {411.625f, 808.188f},
                        {1064.62f, 135.688f},
                        {1042.63f, 585.187f}};

const Vec2D kLoop[4] = {{635.625f, 614.687f},
                        {171.625f, 236.188f},
                        {1064.62f, 135.688f},
                        {516.625f, 570.187f}};

const Vec2D kQuad[4] = {{460.625f, 557.187f}, {707.121f, 209.688f}, {779.628f, 577.687f}};

static void map_pts(const Mat2D& m, Vec2D out[], const Vec2D in[], int n)
{
    for (int i = 0; i < n; ++i)
    {
        out[i] = m * in[i];
    }
}

static float wangs_formula_quadratic_reference_impl(float precision, const Vec2D p[3])
{
    float k = (2 * 1) / 8.f * precision;
    return sqrtf(k * (p[0] - p[1] * 2 + p[2]).length());
}

static float wangs_formula_cubic_reference_impl(float precision, const Vec2D p[4])
{
    float k = (3 * 2) / 8.f * precision;
    return sqrtf(k *
                 std::max((p[0] - p[1] * 2 + p[2]).length(), (p[1] - p[2] * 2 + p[3]).length()));
}

static void chop_quad_at(const Vec2D src[3], Vec2D dst[5], float t)
{
    assert(t > 0 && t < 1);

    float2 p0 = simd::load2f(&src[0].x);
    float2 p1 = simd::load2f(&src[1].x);
    float2 p2 = simd::load2f(&src[2].x);
    float2 tt(t);

    float2 p01 = simd::mix(p0, p1, tt);
    float2 p12 = simd::mix(p1, p2, tt);

    simd::store(&dst[0].x, p0);
    simd::store(&dst[1].x, p01);
    simd::store(&dst[2].x, simd::mix(p01, p12, tt));
    simd::store(&dst[3].x, p12);
    simd::store(&dst[4].x, p2);
}

static Vec2D eval_quad_at(const Vec2D src[3], float t)
{
    assert(t > 0 && t < 1);

    float2 p0 = simd::load2f(&src[0].x);
    float2 p1 = simd::load2f(&src[1].x);
    float2 p2 = simd::load2f(&src[2].x);
    float2 tt(t);

    float2 p01 = simd::mix(p0, p1, tt);
    float2 p12 = simd::mix(p1, p2, tt);
    float2 p012 = simd::mix(p01, p12, tt);

    Vec2D vec;
    simd::store(&vec.x, p012);
    return vec;
}

// Returns number of segments for linearized quadratic rational. This is an analogue
// to Wang's formula, taken from:
//
//   J. Zheng, T. Sederberg. "Estimating Tessellation Parameter Intervals for
//   Rational Curves and Surfaces." ACM Transactions on Graphics 19(1). 2000.
// See Thm 3, Corollary 1.
//
// Input points should be in projected space.
static float wangs_formula_conic_reference_impl(float precision, const Vec2D P[3], const float w)
{
    // Compute center of bounding box in projected space
    float min_x = P[0].x, max_x = min_x, min_y = P[0].y, max_y = min_y;
    for (int i = 1; i < 3; i++)
    {
        min_x = std::min(min_x, P[i].x);
        max_x = std::max(max_x, P[i].x);
        min_y = std::min(min_y, P[i].y);
        max_y = std::max(max_y, P[i].y);
    }
    const Vec2D C = Vec2D(0.5f * (min_x + max_x), 0.5f * (min_y + max_y));

    // Translate control points and compute max length
    Vec2D tP[3] = {P[0] - C, P[1] - C, P[2] - C};
    float max_len = 0;
    for (int i = 0; i < 3; i++)
    {
        max_len = std::max(max_len, tP[i].length());
    }
    assert(max_len > 0);

    // Compute delta = parametric step size of linearization
    const float eps = 1 / precision;
    const float r_minus_eps = std::max(0.f, max_len - eps);
    const float min_w = std::min(w, 1.f);
    const float numer = 4 * min_w * eps;
    const float denom =
        (tP[2] - tP[1] * 2 * w + tP[0]).length() + r_minus_eps * std::abs(1 - 2 * w + 1);
    const float delta = sqrtf(numer / denom);

    // Return corresponding num segments in the interval [tmin,tmax]
    constexpr float tmin = 0, tmax = 1;
    assert(delta > 0);
    return (tmax - tmin) / delta;
}

static float frand() { return rand() / static_cast<float>(RAND_MAX); }

static float frand_range(float min, float max) { return min + frand() * (max - min); }

static void for_random_matrices(std::function<void(const Mat2D&)> f)
{
    srand(0);

    Mat2D m{};
    f(m);

    for (int i = -10; i <= 30; ++i)
    {
        for (int j = -10; j <= 30; ++j)
        {
            m[0] = std::ldexp(1 + frand(), i);
            m[1] = 0;
            m[2] = 0;
            m[3] = std::ldexp(1 + frand(), j);
            f(m);

            m[0] = std::ldexp(1 + frand(), i);
            m[1] = std::ldexp(1 + frand(), (j + i) / 2);
            m[2] = std::ldexp(1 + frand(), (j + i) / 2);
            m[3] = std::ldexp(1 + frand(), j);
            f(m);
        }
    }
}

static void for_random_beziers(int numPoints,
                               std::function<void(const Vec2D[])> f,
                               int maxExponent = 30)
{
    srand(0);

    assert(numPoints <= 4);
    Vec2D pts[4];
    for (int i = -10; i <= maxExponent; ++i)
    {
        for (int j = 0; j < numPoints; ++j)
        {
            pts[j] = {std::ldexp(1 + frand(), i), std::ldexp(1 + frand(), i)};
        }
        f(pts);
    }
}

// Ensure the optimized "*_log2" versions return the same value as ceil(std::log2(f)).
TEST_CASE("wangs_formula_log2", "[wangs_formula]")
{
    // Constructs a cubic such that the 'length' term in wang's formula == term.
    //
    //     f = sqrt(k * length(max(abs(p0 - p1*2 + p2),
    //                             abs(p1 - p2*2 + p3))));
    auto setupCubicLengthTerm = [](int seed, Vec2D pts[], float term) {
        memset(pts, 0, sizeof(Vec2D) * 4);

        Vec2D term2d = (seed & 1) ? Vec2D(term, 0) : Vec2D(.5f, std::sqrt(3) / 2) * term;
        seed >>= 1;

        if (seed & 1)
        {
            term2d.x = -term2d.x;
        }
        seed >>= 1;

        if (seed & 1)
        {
            std::swap(term2d.x, term2d.y);
        }
        seed >>= 1;

        switch (seed % 4)
        {
            case 0:
                pts[0] = term2d;
                pts[3] = term2d * .75f;
                return;
            case 1:
                pts[1] = term2d * -.5f;
                return;
            case 2:
                pts[1] = term2d * -.5f;
                return;
            case 3:
                pts[3] = term2d;
                pts[0] = term2d * .75f;
                return;
        }
    };

    // Constructs a quadratic such that the 'length' term in wang's formula == term.
    //
    //     f = sqrt(k * length(p0 - p1*2 + p2));
    auto setupQuadraticLengthTerm = [](int seed, Vec2D pts[], float term) {
        memset(pts, 0, sizeof(Vec2D) * 3);

        Vec2D term2d = (seed & 1) ? Vec2D(term, 0) : Vec2D(.5f, std::sqrt(3) / 2) * term;
        seed >>= 1;

        if (seed & 1)
        {
            term2d.x = -term2d.x;
        }
        seed >>= 1;

        if (seed & 1)
        {
            std::swap(term2d.x, term2d.y);
        }
        seed >>= 1;

        switch (seed % 3)
        {
            case 0:
                pts[0] = term2d;
                return;
            case 1:
                pts[1] = term2d * -.5f;
                return;
            case 2:
                pts[2] = term2d;
                return;
        }
    };

    // wangs_formula_cubic and wangs_formula_quadratic both use rsqrt instead of sqrt for speed.
    // Linearization is all approximate anyway, so as long as we are within ~1/2 tessellation
    // segment of the reference value we are good enough.
    constexpr static float kTessellationTolerance = 1 / 128.f;

    for (int level = 0; level < 30; ++level)
    {
        float epsilon = std::ldexp(kEpsilon, level * 2);
        Vec2D pts[4];

        {
            // Test cubic boundaries.
            //     f = sqrt(k * length(max(abs(p0 - p1*2 + p2),
            //                             abs(p1 - p2*2 + p3))));
            constexpr static float k = (3 * 2) / (8 * (1.f / kPrecision));
            float x = std::ldexp(1, level * 2) / k;
            setupCubicLengthTerm(level << 1, pts, x - epsilon);
            float referenceValue = wangs_formula_cubic_reference_impl(kPrecision, pts);
            REQUIRE(std::ceil(std::log2(referenceValue)) == level);
            float c = wangs_formula::cubic(pts, kPrecision);
            REQUIRE(fuzzy_equal(c / referenceValue, 1, kTessellationTolerance));
            REQUIRE(wangs_formula::cubic_log2(pts, kPrecision) == level);
            setupCubicLengthTerm(level << 1, pts, x + epsilon);
            referenceValue = wangs_formula_cubic_reference_impl(kPrecision, pts);
            REQUIRE(std::ceil(std::log2(referenceValue)) == level + 1);
            c = wangs_formula::cubic(pts, kPrecision);
            REQUIRE(fuzzy_equal(c / referenceValue, 1, kTessellationTolerance));
            REQUIRE(wangs_formula::cubic_log2(pts, kPrecision) == level + 1);
        }

        {
            // Test quadratic boundaries.
            //     f = std::sqrt(k * Length(p0 - p1*2 + p2));
            constexpr static float k = 2 / (8 * (1.f / kPrecision));
            float x = std::ldexp(1, level * 2) / k;
            setupQuadraticLengthTerm(level << 1, pts, x - epsilon);
            float referenceValue = wangs_formula_quadratic_reference_impl(kPrecision, pts);
            REQUIRE(std::ceil(std::log2(referenceValue)) == level);
            float q = wangs_formula::quadratic(pts, kPrecision);
            REQUIRE(fuzzy_equal(q / referenceValue, 1, kTessellationTolerance));
            REQUIRE(wangs_formula::quadratic_log2(pts, kPrecision) == level);
            setupQuadraticLengthTerm(level << 1, pts, x + epsilon);
            referenceValue = wangs_formula_quadratic_reference_impl(kPrecision, pts);
            REQUIRE(std::ceil(std::log2(referenceValue)) == level + 1);
            q = wangs_formula::quadratic(pts, kPrecision);
            REQUIRE(fuzzy_equal(q / referenceValue, 1, kTessellationTolerance));
            REQUIRE(wangs_formula::quadratic_log2(pts, kPrecision) == level + 1);
        }
    }

    auto check_cubic_log2 = [&](const Vec2D* pts) {
        float f = std::max(1.f, wangs_formula_cubic_reference_impl(kPrecision, pts));
        int f_log2 = wangs_formula::cubic_log2(pts, kPrecision);
        REQUIRE(ceilf(std::log2(f)) == f_log2);
        float c = std::max(1.f, wangs_formula::cubic(pts, kPrecision));
        REQUIRE(fuzzy_equal(c / f, 1, kTessellationTolerance));
    };

    auto check_quadratic_log2 = [&](const Vec2D* pts) {
        float f = std::max(1.f, wangs_formula_quadratic_reference_impl(kPrecision, pts));
        int f_log2 = wangs_formula::quadratic_log2(pts, kPrecision);
        REQUIRE(ceilf(std::log2(f)) == f_log2);
        float q = std::max(1.f, wangs_formula::quadratic(pts, kPrecision));
        REQUIRE(fuzzy_equal(q / f, 1, kTessellationTolerance));
    };

    for_random_matrices([&](const Mat2D& m) {
        Vec2D pts[4 + 999];
        map_pts(m, pts, kSerp, 4);
        check_cubic_log2(pts);

        map_pts(m, pts, kLoop, 4);
        check_cubic_log2(pts);

        map_pts(m, pts, kQuad, 3);
        check_quadratic_log2(pts);
    });

    for_random_beziers(4, [&](const Vec2D pts[]) { check_cubic_log2(pts); });

    for_random_beziers(3, [&](const Vec2D pts[]) { check_quadratic_log2(pts); });
}

static void check_cubic_log2_with_transform(const Vec2D* pts, const Mat2D& m)
{
    Vec2D ptsXformed[4];
    map_pts(m, ptsXformed, pts, 4);
    int expected = wangs_formula::cubic_log2(ptsXformed, kPrecision);
    int actual = wangs_formula::cubic_log2(pts, kPrecision, wangs_formula::VectorXform(m));
    REQUIRE(actual == expected);
};

static void check_quadratic_log2_with_transform(const Vec2D* pts, const Mat2D& m)
{
    Vec2D ptsXformed[3];
    map_pts(m, ptsXformed, pts, 3);
    int expected = wangs_formula::quadratic_log2(ptsXformed, kPrecision);
    int actual = wangs_formula::quadratic_log2(pts, kPrecision, wangs_formula::VectorXform(m));
    REQUIRE(actual == expected);
};

// Ensure using transformations gives the same result as pre-transforming all points.
TEST_CASE("wangs_formula_vectorXforms", "[wangs_formula]")
{
    for_random_matrices([&](const Mat2D& m) {
        check_cubic_log2_with_transform(kSerp, m);
        check_cubic_log2_with_transform(kLoop, m);
        check_quadratic_log2_with_transform(kQuad, m);

        for_random_beziers(4, [&](const Vec2D pts[]) { check_cubic_log2_with_transform(pts, m); });

        for_random_beziers(3,
                           [&](const Vec2D pts[]) { check_quadratic_log2_with_transform(pts, m); });
    });
}

TEST_CASE("wangs_formula_worst_case_cubic", "[wangs_formula]")
{
    {
        Vec2D worstP[] = {{0, 0}, {100, 100}, {0, 0}, {0, 0}};
        REQUIRE(wangs_formula::worst_case_cubic(100, 100, kPrecision) ==
                wangs_formula_cubic_reference_impl(kPrecision, worstP));
        REQUIRE(wangs_formula::worst_case_cubic_log2(100, 100, kPrecision) ==
                wangs_formula::cubic_log2(worstP, kPrecision));
    }
    {
        Vec2D worstP[] = {{100, 100}, {100, 100}, {200, 200}, {100, 100}};
        REQUIRE(wangs_formula::worst_case_cubic(100, 100, kPrecision) ==
                wangs_formula_cubic_reference_impl(kPrecision, worstP));
        REQUIRE(wangs_formula::worst_case_cubic_log2(100, 100, kPrecision) ==
                wangs_formula::cubic_log2(worstP, kPrecision));
    }
    auto check_worst_case_cubic = [&](const Vec2D* pts) {
        float2 min = simd::load2f(&pts[0].x), max = simd::load2f(&pts[0].x);
        for (int i = 1; i < 4; ++i)
        {
            min = simd::min(min, simd::load2f(&pts[i].x));
            max = simd::max(max, simd::load2f(&pts[i].x));
        }
        float2 size = max - min;
        float worst = wangs_formula::worst_case_cubic(size.x, size.y, kPrecision);
        int worst_log2 = wangs_formula::worst_case_cubic_log2(size.x, size.y, kPrecision);
        float actual = wangs_formula_cubic_reference_impl(kPrecision, pts);
        REQUIRE(worst >= actual);
        REQUIRE(std::ceil(std::log2(std::max(1.f, worst))) == worst_log2);
    };
    for (int i = 0; i < 100; ++i)
    {
        for_random_beziers(4, [&](const Vec2D pts[]) { check_worst_case_cubic(pts); });
    }
    // Make sure overflow saturates at infinity (not NaN).
    constexpr static float inf = std::numeric_limits<float>::infinity();
    REQUIRE(wangs_formula::worst_case_cubic_pow4(inf, inf, kPrecision) == inf);
    REQUIRE(wangs_formula::worst_case_cubic(inf, inf, kPrecision) == inf);
}

// Ensure Wang's formula for quads produces max error within tolerance.
TEST_CASE("wangs_formula_quad_within_tol", "[wangs_formula]")
{
    // Wang's formula and the quad math starts to lose precision with very large
    // coordinate values, so limit the magnitude a bit to prevent test failures
    // due to loss of precision.
    constexpr int maxExponent = 15;
    for_random_beziers(
        3,
        [](const Vec2D pts[]) {
            const int nsegs = static_cast<int>(
                std::ceil(wangs_formula_quadratic_reference_impl(kPrecision, pts)));

            const float tdelta = 1.f / nsegs;
            for (int j = 0; j < nsegs; ++j)
            {
                const float tmin = j * tdelta, tmax = (j + 1) * tdelta;

                // Get section of quad in [tmin,tmax]
                const Vec2D* sectionPts;
                Vec2D tmp0[5];
                Vec2D tmp1[5];
                if (tmin == 0)
                {
                    if (tmax == 1)
                    {
                        sectionPts = pts;
                    }
                    else
                    {
                        chop_quad_at(pts, tmp0, tmax);
                        sectionPts = tmp0;
                    }
                }
                else
                {
                    chop_quad_at(pts, tmp0, tmin);
                    if (tmax == 1)
                    {
                        sectionPts = tmp0 + 2;
                    }
                    else
                    {
                        chop_quad_at(tmp0 + 2, tmp1, (tmax - tmin) / (1 - tmin));
                        sectionPts = tmp1;
                    }
                }

                // For quads, max distance from baseline is always at t=0.5.
                Vec2D p;
                p = eval_quad_at(sectionPts, 0.5f);

                // Get distance of p to baseline
                const Vec2D n = {sectionPts[2].y - sectionPts[0].y,
                                 sectionPts[0].x - sectionPts[2].x};
                const float d = std::abs(Vec2D::dot(p - sectionPts[0], n)) / n.length();

                // Check distance is within specified tolerance
                REQUIRE(d <= (1.f / kPrecision) + 1e-2f);
            }
        },
        maxExponent);
}

// Ensure the specialized version for rational quads reduces to regular Wang's
// formula when all weights are equal to one
TEST_CASE("wangs_formula_rational_quad_reduces", "[wangs_formula]")
{
    constexpr static float kTessellationTolerance = 1 / 128.f;

    for (int i = 0; i < 100; ++i)
    {
        for_random_beziers(3, [](const Vec2D pts[]) {
            const float rational_nsegs = wangs_formula::conic(kPrecision, pts, 1.f);
            const float integral_nsegs = wangs_formula_quadratic_reference_impl(kPrecision, pts);
            REQUIRE(fuzzy_equal(rational_nsegs, integral_nsegs, kTessellationTolerance));
        });
    }
}

// Ensure the rational quad version (used for conics) produces max error within tolerance.
TEST_CASE("wangs_formula_conic_within_tol", "[wangs_formula]")
{
    constexpr int maxExponent = 24;

    srand(0);

    // Single-precision functions in SkConic/SkGeometry lose too much accuracy with
    // large-magnitude curves and large weights for this test to pass.
    using Sk2d = simd::gvec<double, 2>;
    const auto eval_conic = [](const Vec2D pts[3], double w, double t) -> Sk2d {
        const auto eval = [](Sk2d A, Sk2d B, Sk2d C, double t) -> Sk2d {
            return (A * t + B) * t + C;
        };

        const Sk2d p0 = {pts[0].x, pts[0].y};
        const Sk2d p1 = {pts[1].x, pts[1].y};
        const Sk2d p1w = p1 * w;
        const Sk2d p2 = {pts[2].x, pts[2].y};
        Sk2d numer = eval(p2 - p1w * 2.0 + p0, (p1w - p0) * 2.0, p0, t);

        Sk2d denomC = {1, 1};
        Sk2d denomB = {2 * (w - 1), 2 * (w - 1)};
        Sk2d denomA = {-2 * (w - 1), -2 * (w - 1)};
        Sk2d denom = eval(denomA, denomB, denomC, t);
        return numer / denom;
    };

    const auto dot = [](const Sk2d& a, const Sk2d& b) -> double {
        return a[0] * b[0] + a[1] * b[1];
    };

    const auto length = [](const Sk2d& p) -> double { return sqrt(p[0] * p[0] + p[1] * p[1]); };

    for (int i = -10; i <= 10; ++i)
    {
        const float w = std::ldexp(1 + frand(), i);
        for_random_beziers(
            3,
            [&](const Vec2D pts[]) {
                const int nsegs = static_cast<int>(ceilf(wangs_formula::conic(kPrecision, pts, w)));

                const float tdelta = 1.f / nsegs;
                for (int j = 0; j < nsegs; ++j)
                {
                    const float tmin = j * tdelta, tmax = (j + 1) * tdelta,
                                tmid = 0.5f * (tmin + tmax);

                    Sk2d p0, p1, p2;
                    p0 = eval_conic(pts, w, tmin);
                    p1 = eval_conic(pts, w, tmid);
                    p2 = eval_conic(pts, w, tmax);

                    // Get distance of p1 to baseline (p0, p2).
                    const Sk2d n = {p2[1] - p0[1], p0[0] - p2[0]};
                    assert(length(n) != 0);
                    const double d = std::abs(dot(p1 - p0, n)) / length(n);

                    // Check distance is within tolerance
                    REQUIRE(d <= (1.0 / kPrecision) + kEpsilon);
                    REQUIRE(d <= (1.0 / kPrecision) + kEpsilon);
                }
            },
            maxExponent);
    }
}

// Ensure the vectorized conic version equals the reference implementation
TEST_CASE("wangs_formula_conic_matches_reference", "[wangs_formula]")
{
    srand(0);

    for (int i = -10; i <= 10; ++i)
    {
        const float w = std::ldexp(1 + frand(), i);
        for_random_beziers(3, [w](const Vec2D pts[]) {
            const float ref_nsegs = wangs_formula_conic_reference_impl(kPrecision, pts, w);
            const float nsegs = wangs_formula::conic(kPrecision, pts, w);

            // Because the Gr version may implement the math differently for performance,
            // allow different slack in the comparison based on the rough scale of the answer.
            const float cmpThresh = ref_nsegs * (1.f / (1 << 20));
            REQUIRE(fuzzy_equal(ref_nsegs, nsegs, cmpThresh));
        });
    }
}

// Ensure using transformations gives the same result as pre-transforming all points.
TEST_CASE("wangs_formula_conic_vectorXforms", "[wangs_formula]")
{
    srand(0);

    auto check_conic_with_transform = [&](const Vec2D* pts, float w, const Mat2D& m) {
        Vec2D ptsXformed[3];
        map_pts(m, ptsXformed, pts, 3);
        float expected = wangs_formula::conic(kPrecision, ptsXformed, w);
        float actual = wangs_formula::conic(kPrecision, pts, w, wangs_formula::VectorXform(m));
        REQUIRE(actual == Approx(expected).margin(1e-4));
    };

    for (int i = -10; i <= 10; ++i)
    {
        const float w = std::ldexp(1 + frand(), i);
        for_random_beziers(3, [&](const Vec2D pts[]) {
            check_conic_with_transform(pts, w, Mat2D());
            check_conic_with_transform(
                pts,
                w,
                Mat2D::fromScale(frand_range(-10, 10), frand_range(-10, 10)));

            // Random 2x2 matrix
            Mat2D m;
            m[0] = frand_range(-10, 10);
            m[1] = frand_range(-10, 10);
            m[2] = frand_range(-10, 10);
            m[3] = frand_range(-10, 10);
            check_conic_with_transform(pts, w, m);
        });
    }
}
} // namespace rive
