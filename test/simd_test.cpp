/*
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

// Check that ?: works on vector and scalar conditions.
TEST_CASE("ternary-operator", "[simd]")
{
    // Vector condition.
    float4 f4 = int4{1, 2, 3, 4} < int4{4, 3, 2, 1} ? float4(-1) : 1.f;
    CHECK(simd::all(f4 == float4{-1, -1, 1, 1}));

    // In vector, -1 is true, 0 is false.
    uint2 u2 = int2{0, -1} ? uint2{1, 2} : uint2{3, 4};
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
    CHECK(simd::all(float4{-std::numeric_limits<float>::epsilon(),
                           -std::numeric_limits<float>::denorm_min(),
                           -std::numeric_limits<float>::max(),
                           -kInf} == float4{-std::numeric_limits<float>::epsilon(),
                                            -std::numeric_limits<float>::denorm_min(),
                                            -std::numeric_limits<float>::max(),
                                            -kInf}

                    ));
    float2 nan2 = simd::abs(float2{kNaN, -kNaN});
    CHECK(std::isnan(nan2.x));
    CHECK(std::isnan(nan2.y));
    CHECK(simd::all(simd::abs(int4{7, -8, 9, -10}) == int4{7, 8, 9, 10}));
    // abs(INT_MIN) returns INT_MIN.
    CHECK(
        simd::all(simd::abs(int2{-std::numeric_limits<int32_t>::max(),
                                 std::numeric_limits<int32_t>::min()}) ==
                  int2{std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min()}));
}

// Check simd::dot.
TEST_CASE("dot", "[simd]")
{
    CHECK(simd::dot(int2{0, 1}, int2{1, 0}) == 0);
    CHECK(simd::dot(int2{1, 0}, int2{0, 1}) == 0);
    CHECK(simd::dot(int2{1, 1}, int2{1, -1}) == 0);
    CHECK(simd::dot(int2{1, 1}, int2{1, 1}) == 2);
    CHECK(simd::dot(int2{1, 1}, int2{-1, -1}) == -2);
    CHECK(simd::dot(simd::gvec<int, 3>{1, 2, 3}, simd::gvec<int, 3>{1, 2, 3}) == 14);
    CHECK(simd::dot(int4{1, 2, 3, 4}, int4{1, 2, 3, 4}) == 30);
    CHECK(simd::dot(ivec<5>{1, 2, 3, 4, 5}, ivec<5>{1, 2, 3, 4, 5}) == 55);
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
    float f = static_cast<float>(rand()) / RAND_MAX;
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
