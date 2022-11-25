/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * "fast_acos" test imported from skia:tests/SkVxTest.cpp
 *
 * Copyright 2022 Rive
 */

#include <catch.hpp>

#include "rive/math/math_types.hpp"
#include "rive/math/simd.hpp"
#include <limits>

namespace rive
{

constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

// Check simd::any.
TEST_CASE("any", "[simd]")
{
    CHECK(!simd::any(int4{0, 0, 0, 0}));
    CHECK(simd::any(int4{-1, 0, 0, 0}));
    CHECK(simd::any(int4{0, -1, 0, 0}));
    CHECK(simd::any(int4{0, 0, -1, 0}));
    CHECK(simd::any(int4{0, 0, 0, -1}));
    CHECK(!simd::any(ivec<3>{0, 0, 0}));
    CHECK(simd::any(ivec<3>{-1, 0, 0}));
    CHECK(simd::any(ivec<3>{0, -1, 0}));
    CHECK(simd::any(ivec<3>{0, 0, -1}));
    CHECK(!simd::any(int2{0, 0}));
    CHECK(simd::any(int2{-1, 0}));
    CHECK(simd::any(int2{0, -1}));
    CHECK(!simd::any(ivec<1>{0}));
    CHECK(simd::any(ivec<1>{-1}));
}

// Check simd::all.
TEST_CASE("all", "[simd]")
{
    CHECK(simd::all(int4{-1, -1, -1, -1}));
    CHECK(!simd::all(int4{0, -1, -1, -1}));
    CHECK(!simd::all(int4{-1, 0, -1, -1}));
    CHECK(!simd::all(int4{-1, -1, 0, -1}));
    CHECK(!simd::all(int4{-1, -1, -1, 0}));
    CHECK(simd::all(ivec<3>{-1, -1, -1}));
    CHECK(!simd::all(ivec<3>{0, -1, -1}));
    CHECK(!simd::all(ivec<3>{-1, 0, -1}));
    CHECK(!simd::all(ivec<3>{-1, -1, 0}));
    CHECK(simd::all(int2{-1, -1}));
    CHECK(!simd::all(int2{0, -1}));
    CHECK(!simd::all(int2{-1, 0}));
    CHECK(simd::all(ivec<1>{-1}));
    CHECK(!simd::all(ivec<1>{0}));
}

// Verify the simd float types are IEEE 754 compliant for infinity and NaN.
TEST_CASE("ieee-compliance", "[simd]")
{
    float4 test = float4{1, -kInf, 1, 4} / float4{0, 2, kInf, 4};
    CHECK(simd::all(test == float4{kInf, -kInf, 0, 1}));

    // Inf * Inf == Inf
    test = float4{kInf, -kInf, kInf, -kInf} * float4{kInf, kInf, -kInf, -kInf};
    CHECK(simd::all(test == float4{kInf, -kInf, -kInf, kInf}));

    // Inf/0 == Inf, 0/Inf == 0
    test = float4{kInf, -kInf, 0, 0} / float4{0, 0, kInf, -kInf};
    CHECK(simd::all(test == float4{kInf, -kInf, 0, 0}));

    // Inf/Inf, 0/0, 0 * Inf, Inf - Inf == NaN
    test = {kInf, 0, 0, kInf};
    test.xy /= float2{kInf, 0};
    test.z *= kInf;
    test.w -= kInf;
    for (int i = 0; i < 4; ++i)
    {
        CHECK(std::isnan(test[i]));
    }
    // NaN always fails comparisons.
    CHECK(!simd::any(test == test));
    CHECK(simd::all(test != test));
    CHECK(!simd::any(test <= test));
    CHECK(!simd::any(test >= test));
    CHECK(!simd::any(test < test));
    CHECK(!simd::any(test > test));

    // Inf + Inf == Inf, Inf + -Inf == NaN
    test = float4{kInf, -kInf, kInf, -kInf} + float4{kInf, -kInf, -kInf, kInf};
    CHECK(simd::all(test.xy == float2{kInf, -kInf}));
    CHECK(!simd::any(test.zw == test.zw)); // NaN
}

// Check simd::if_then_else.
TEST_CASE("ternary-operator", "[simd]")
{
    // Vector condition.
    float4 f4 = simd::if_then_else(int4{1, 2, 3, 4} < int4{4, 3, 2, 1}, float4(-1), float4(1));
    CHECK(simd::all(f4 == float4{-1, -1, 1, 1}));

    // In vector, -1 is true, 0 is false.
    uint2 u2 = simd::if_then_else(int2{0, -1}, uint2{1, 2}, uint2{3, 4});
    CHECK(simd::all(u2 == uint2{3, 2}));

    // Scalar condition.
    f4 = u2.x == u2.y ? float4{1, 2, 3, 4} : float4{5, 6, 7, 8};
    CHECK(simd::all(f4 == float4{5, 6, 7, 8}));
}

// Check simd::min/max compliance.
TEST_CASE("min-max", "[simd]")
{
    float4 f4 = simd::min(float4{1, 2, 3, 4}, float4{4, 3, 2});
    CHECK(simd::all(f4 == float4{1, 2, 2, 0}));
    f4 = simd::max(float4{1, 2, 3, 4}, float4{4, 3, 2});
    CHECK(simd::all(f4 == float4{4, 3, 3, 4}));

    int2 i2 = simd::max(int2(-1), int2{-2});
    CHECK(simd::all(i2 == int2{-1, 0}));
    i2 = simd::min(int2(-1), int2{-2});
    CHECK(simd::all(i2 == int2{-2, -1}));

    // Infinity works as expected.
    f4 = simd::min(float4{100, -kInf, -kInf, kInf}, float4{kInf, 100, kInf, -kInf});
    CHECK(simd::all(f4 == float4{100, -kInf, -kInf, -kInf}));
    f4 = simd::max(float4{100, -kInf, -kInf, kInf}, float4{kInf, 100, kInf, -kInf});
    CHECK(simd::all(f4 == float4{kInf, 100, kInf, kInf}));

    // If a or b is NaN, min returns whichever is not NaN.
    f4 = simd::min(float4{1, kNaN, 2, kNaN}, float4{kNaN, 1, 1, kNaN});
    CHECK(simd::all(f4.xyz == 1));
    CHECK(std::isnan(f4.w));
    f4 = simd::max(float4{1, kNaN, 2, kNaN}, float4{kNaN, 1, 1, kNaN});
    CHECK(simd::all(f4.xyz == vec<3>{1, 1, 2}));
    CHECK(std::isnan(f4.w));

    // simd::min/max differs from std::min/max when the first argument is NaN.
    CHECK(simd::min<float, 1>(kNaN, 1).x == 1);
    CHECK(std::isnan(std::min<float>(kNaN, 1)));
    CHECK(simd::max<float, 1>(kNaN, 1).x == 1);
    CHECK(std::isnan(std::max<float>(kNaN, 1)));

    // simd::min/max is equivalent std::min/max when the second argument is NaN.
    CHECK(simd::min<float, 1>(1, kNaN).x == std::min<float>(1, kNaN));
    CHECK(simd::max<float, 1>(1, kNaN).x == std::max<float>(1, kNaN));
}

// Check simd::clamp.
TEST_CASE("clamp", "[simd]")
{
    CHECK(simd::all(simd::clamp(float4{1, 2, kInf, -kInf},
                                float4{2, 1, kInf, 0},
                                float4{3, 1, kInf, kInf}) == float4{2, 1, kInf, 0}));
    CHECK(simd::all(simd::clamp(float4{1, kNaN, kInf, -kInf},
                                float4{kNaN, 2, kNaN, 0},
                                float4{kNaN, 3, kInf, kNaN}) == float4{1, 2, kInf, 0}));
    float4 f4 = simd::clamp(float4{1, kNaN, kNaN, kNaN},
                            float4{kNaN, 1, kNaN, kNaN},
                            float4{kNaN, kNaN, 1, kNaN});
    CHECK(simd::all(f4.xyz == 1));
    CHECK(std::isnan(f4.w));

    // Returns lo if x == NaN, but std::clamp() returns NaN.
    CHECK(simd::clamp<float, 1>(kNaN, 1, 2).x == 1);
    CHECK(std::clamp<float>(kNaN, 1, 2) != 1);
    CHECK(std::isnan(std::clamp<float>(kNaN, 1, 2)));

    // Returns hi if hi <= lo.
    CHECK(simd::clamp<float, 1>(3, 2, 1).x == 1);
    CHECK(simd::clamp<float, 1>(kNaN, 2, 1).x == 1);
    CHECK(simd::clamp<float, 1>(kNaN, kNaN, 1).x == 1);

    // Ignores hi and/or lo if they are NaN.
    CHECK(simd::clamp<float, 1>(3, 4, kNaN).x == 4);
    CHECK(simd::clamp<float, 1>(3, 2, kNaN).x == 3);
    CHECK(simd::clamp<float, 1>(3, kNaN, 2).x == 2);
    CHECK(simd::clamp<float, 1>(3, kNaN, 4).x == 3);
    CHECK(simd::clamp<float, 1>(3, kNaN, kNaN).x == 3);
}

// Check simd::abs.
TEST_CASE("abs", "[simd]")
{
    CHECK(simd::all(simd::abs(float4{-1, 2, -3, 4}) == float4{1, 2, 3, 4}));
    CHECK(simd::all(simd::abs(float2{-5, 6}) == float2{5, 6}));
    CHECK(simd::all(simd::abs(float2{-0, 0}) == float2{0, 0}));
    CHECK(simd::all(float4{-std::numeric_limits<float>::epsilon(),
                           -std::numeric_limits<float>::denorm_min(),
                           -std::numeric_limits<float>::max(),
                           -kInf} == float4{-std::numeric_limits<float>::epsilon(),
                                            -std::numeric_limits<float>::denorm_min(),
                                            -std::numeric_limits<float>::max(),
                                            -kInf}

                    ));
    float2 nan2 = simd::abs(float2{kNaN, -kNaN});
    CHECK(simd::all(simd::isnan(nan2)));
    CHECK(simd::all(simd::abs(int4{7, -8, 9, -10}) == int4{7, 8, 9, 10}));
    CHECK(simd::all(simd::abs(int2{0, -0}) == int2{0, 0}));
    // abs(INT_MIN) returns INT_MIN.
    CHECK(
        simd::all(simd::abs(int2{-std::numeric_limits<int32_t>::max(),
                                 std::numeric_limits<int32_t>::min()}) ==
                  int2{std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min()}));
}

// Check simd::floor.
TEST_CASE("floor", "[simd]")
{
    CHECK(simd::all(simd::floor(float4{-1.9f, 1.9f, 2, -2}) == float4{-2, 1, 2, -2}));
    CHECK(simd::all(simd::floor(float2{kInf, -kInf}) == float2{kInf, -kInf}));
    CHECK(simd::all(simd::isnan(simd::floor(float2{kNaN, -kNaN}))));
}

// Check simd::ceil.
TEST_CASE("ceil", "[simd]")
{
    CHECK(simd::all(simd::ceil(float4{-1.9f, 1.9f, 2, -2}) == float4{-1, 2, 2, -2}));
    CHECK(simd::all(simd::ceil(float2{kInf, -kInf}) == float2{kInf, -kInf}));
    CHECK(simd::all(simd::isnan(simd::ceil(float2{kNaN, -kNaN}))));
}

// Check simd::sqrt.
TEST_CASE("sqrt", "[simd]")
{
    CHECK(simd::all(simd::sqrt(float4{1, 4, 9, 16}) == float4{1, 2, 3, 4}));
    CHECK(simd::all(simd::sqrt(float2{25, 36}) == float2{5, 6}));
    CHECK(simd::all(simd::sqrt(vec<1>{36}) == vec<1>{6}));
    CHECK(simd::all(simd::sqrt(vec<5>{49, 64, 81, 100, 121}) == vec<5>{7, 8, 9, 10, 11}));
    CHECK(simd::all(simd::isnan(simd::sqrt(float4{-1, -kInf, kNaN, -2}))));
    CHECK(simd::all(simd::sqrt(vec<3>{kInf, 0, 1}) == vec<3>{kInf, 0, 1}));
}

static bool check_fast_acos(float x, float fast_acos_x)
{
    float acosf_x = acosf(x);
    float error = acosf_x - fast_acos_x;
    if (!(fabsf(error) <= SIMD_FAST_ACOS_MAX_ERROR))
    {
        auto rad2deg = [](float rad) { return rad * 180 / math::PI; };
        fprintf(stderr,
                "Larger-than-expected error from skvx::fast_acos\n"
                "  x=              %f\n"
                "  fast_acos_x=    %f  (%f degrees\n"
                "  acosf_x=        %f  (%f degrees\n"
                "  error=          %f  (%f degrees)\n"
                "  tolerance=      %f  (%f degrees)\n\n",
                x,
                fast_acos_x,
                rad2deg(fast_acos_x),
                acosf_x,
                rad2deg(acosf_x),
                error,
                rad2deg(error),
                SIMD_FAST_ACOS_MAX_ERROR,
                rad2deg(SIMD_FAST_ACOS_MAX_ERROR));
        CHECK(false);
        return false;
    }
    return true;
}

TEST_CASE("fast_acos", "[simd]")
{
    float4 boundaries = simd::fast_acos(float4{-1, 0, 1, 0});
    check_fast_acos(-1, boundaries[0]);
    check_fast_acos(0, boundaries[1]);
    check_fast_acos(+1, boundaries[2]);

    // Select a distribution of starting points around which to begin testing fast_acos. These
    // fall roughly around the known minimum and maximum errors. No need to include -1, 0, or 1
    // since those were just tested above. (Those are tricky because 0 is an inflection and the
    // derivative is infinite at 1 and -1.)
    using float8 = vec<8>;
    float8 x = {-.99f, -.8f, -.4f, -.2f, .2f, .4f, .8f, .99f};

    // Converge at the various local minima and maxima of "fast_acos(x) - cosf(x)" and verify that
    // fast_acos is always within "kTolerance" degrees of the expected answer.
    float8 err_;
    for (int iter = 0; iter < 10; ++iter)
    {
        // Run our approximate inverse cosine approximation.
        auto fast_acos_x = simd::fast_acos(x);

        // Find d/dx(error)
        //    = d/dx(fast_acos(x) - acos(x))
        //    = (f'g - fg')/gg + 1/sqrt(1 - x^2), [where f = bx^3 + ax, g = dx^4 + cx^2 + 1]
        float8 xx = x * x;
        float8 a = -0.939115566365855f;
        float8 b = 0.9217841528914573f;
        float8 c = -1.2845906244690837f;
        float8 d = 0.295624144969963174f;
        float8 f = (b * xx + a) * x;
        float8 f_ = 3 * b * xx + a;
        float8 g = (d * xx + c) * xx + 1;
        float8 g_ = (4 * d * xx + 2 * c) * x;
        float8 gg = g * g;
        float8 q = simd::sqrt(1 - xx);
        err_ = (f_ * g - f * g_) / gg + 1 / q;

        // Find d^2/dx^2(error)
        //    = ((f''g - fg'')g^2 - (f'g - fg')2gg') / g^4 + x(1 - x^2)^(-3/2)
        //    = ((f''g - fg'')g - (f'g - fg')2g') / g^3 + x(1 - x^2)^(-3/2)
        float8 f__ = 6 * b * x;
        float8 g__ = 12 * d * xx + 2 * c;
        float8 err__ =
            ((f__ * g - f * g__) * g - (f_ * g - f * g_) * 2 * g_) / (gg * g) + x / ((1 - xx) * q);

#if 0
        SkDebugf("\n\niter %i\n", iter);
#endif
        // Ensure each lane's approximation is within maximum error.
        for (int j = 0; j < 8; ++j)
        {
#if 0
            SkDebugf("x=%f  err=%f  err'=%f  err''=%f\n",
                     x[j], rad2deg(skvx::fast_acos_x[j] - acosf(x[j])),
                     rad2deg(err_[j]), rad2deg(err__[j]));
#endif
            if (!check_fast_acos(x[j], fast_acos_x[j]))
            {
                return;
            }
        }

        // Use Newton's method to update the x values to locations closer to their local minimum or
        // maximum. (This is where d/dx(error) == 0.)
        x -= err_ / err__;
        x = simd::clamp<float, 8>(x, -.99f, .99f);
    }

    // Verify each lane converged to a local minimum or maximum.
    for (int j = 0; j < 8; ++j)
    {
        REQUIRE(math::nearly_zero(err_[j]));
    }

    // Make sure we found all the actual known locations of local min/max error.
    for (float knownRoot : {-0.983536f, -0.867381f, -0.410923f, 0.410923f, 0.867381f, 0.983536f})
    {
        CHECK(simd::any(simd::abs(x - knownRoot) < math::EPSILON));
    }
}

// Check simd::dot.
TEST_CASE("dot", "[simd]")
{
    CHECK(simd::dot(int2{0, 1}, int2{1, 0}) == 0);
    CHECK(simd::dot(uint2{1, 0}, uint2{0, 1}) == 0);
    CHECK(simd::dot(int2{1, 1}, int2{1, -1}) == 0);
    CHECK(simd::dot(uint2{1, 1}, uint2{1, 1}) == 2);
    CHECK(simd::dot(int2{1, 1}, int2{-1, -1}) == -2);
    CHECK(simd::dot(ivec<3>{1, 2, -3}, ivec<3>{1, 2, 3}) == -4);
    CHECK(simd::dot(uvec<3>{1, 2, 3}, uvec<3>{1, 2, 3}) == 14);
    CHECK(simd::dot(int4{1, 2, 3, 4}, int4{1, 2, 3, 4}) == 30);
    CHECK(simd::dot(ivec<5>{1, 2, 3, 4, 5}, ivec<5>{1, 2, 3, 4, -5}) == 5);
    CHECK(simd::dot(uvec<5>{1, 2, 3, 4, 5}, uvec<5>{1, 2, 3, 4, 5}) == 55);

    CHECK(simd::dot(float4{1, 2, 3, 4}, float4{4, 3, 2, 1}) == 20);
    CHECK(simd::dot(vec<3>{1, 2, 3}, vec<3>{3, 2, 1}) == 10);
    CHECK(simd::dot(float2{0, 1}, float2{1, 0}) == 0);
    CHECK(simd::dot(vec<5>{1, 2, 3, 4, 5}, vec<5>{1, 2, 3, 4, 5}) == 55);
}

// Check simd::cross.
TEST_CASE("cross", "[simd]")
{
    CHECK(simd::cross({0, 1}, {0, 1}) == 0);
    CHECK(simd::cross({1, 0}, {1, 0}) == 0);
    CHECK(simd::cross({1, 1}, {1, 1}) == 0);
    CHECK(simd::cross({1, 1}, {1, -1}) == -2);
    CHECK(simd::cross({1, 1}, {-1, 1}) == 2);
}

// Check simd::join
TEST_CASE("join", "[simd]")
{
    CHECK(simd::all(simd::join(int2{1, 2}, int4{3, 4, 5, 6}) == ivec<6>{1, 2, 3, 4, 5, 6}));
    CHECK(simd::all(simd::join(vec<1>{1}, vec<3>{2, 3, 4}) == float4{1, 2, 3, 4}));
}

template <int N> static vec<N> mix_reference_impl(vec<N> a, vec<N> b, float t)
{
    return a * (1 - t) + b * t;
}
template <int N> static vec<N> mix_reference_impl(vec<N> a, vec<N> b, vec<N> t)
{
    return a * (1 - t) + b * t;
}

template <typename T, int N> static bool fuzzy_equal(simd::gvec<T, N> a, simd::gvec<T, N> b)
{
    return simd::all(b - a < 1e-4f);
}

static float frand()
{
    float kMaxBelow1 = math::bit_cast<float>(math::bit_cast<uint32_t>(1.f) - 1);
    float f = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return std::min(kMaxBelow1, f);
}
template <int N> vec<N> vrand()
{
    vec<N> vrand{};
    for (int i = 0; i < N; ++i)
    {
        vrand[i] = frand();
    }
    return vrand;
}

template <int N> void check_mix()
{
    vec<N> a = vrand<N>();
    vec<N> b = vrand<N>();
    float t = frand();
    CHECK(fuzzy_equal(simd::mix(a, b, t), mix_reference_impl(a, b, t)));
    vec<N> tt = vrand<N>();
    CHECK(fuzzy_equal(simd::mix(a, b, tt), mix_reference_impl(a, b, tt)));
}

// Check simd::mix
TEST_CASE("mix", "[simd]")
{
    srand(0);
    check_mix<1>();
    check_mix<2>();
    check_mix<3>();
    check_mix<4>();
    check_mix<5>();
    CHECK(simd::all(simd::mix(float4{1, 2, 3, 4}, float4{5, 6, 7, 8}, 0) == float4{1, 2, 3, 4}));
}

} // namespace rive
