/*
 * Copyright 2022 Rive
 */

#include <catch.hpp>

#include "rive/math/simd.hpp"
#include <limits>

namespace rive {

constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

// Verify the simd float types are IEEE 754 compliant for infinity and NaN.
TEST_CASE("ieee-compliance", "[simd]") {
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
    for (int i = 0; i < 4; ++i) {
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
TEST_CASE("ternary-operator", "[simd]") {
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
TEST_CASE("min-max", "[simd]") {
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

// Check simd::abs.
TEST_CASE("abs", "[simd]") {

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

} // namespace rive
