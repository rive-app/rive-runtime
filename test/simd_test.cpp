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

// Ignore performance warnings in this file about having AVX disabled. We test vectors larger than 4
// and aren't worried about performance.
//
// If we try to use large vectors in other parts of the code with AVX disabled, we definitely still
// want this warning.
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wpsabi"
#endif

#include <catch.hpp>

#include "rive/math/math_types.hpp"
#include "rive/math/simd.hpp"
#include <limits>

#define CHECK_ALL(B) CHECK(simd::all(B))
#define CHECK_ANY(B) CHECK(simd::any(B))

namespace rive
{
constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

// Check simd::any.
TEST_CASE("any", "[simd]")
{
    CHECK(!simd::any(int4{0, 0, 0, 0}));
    CHECK_ANY((int4{-1, 0, 0, 0}));
    CHECK_ANY((int4{0, -1, 0, 0}));
    CHECK_ANY((int4{0, 0, -1, 0}));
    CHECK_ANY((int4{0, 0, 0, -1}));
    CHECK(!simd::any(ivec<3>{0, 0, 0}));
    CHECK_ANY((ivec<3>{-1, 0, 0}));
    CHECK_ANY((ivec<3>{0, -1, 0}));
    CHECK_ANY((ivec<3>{0, 0, -1}));
    CHECK(!simd::any(int2{0, 0}));
    CHECK_ANY((int2{-1, 0}));
    CHECK_ANY((int2{0, -1}));
    CHECK(!simd::any(ivec<1>{0}));
    CHECK_ANY((ivec<1>{-1}));
}

// Check simd::all.
TEST_CASE("all", "[simd]")
{
    CHECK_ALL((int4{-1, -1, -1, -1}));
    CHECK(!simd::all(int4{0, -1, -1, -1}));
    CHECK(!simd::all(int4{-1, 0, -1, -1}));
    CHECK(!simd::all(int4{-1, -1, 0, -1}));
    CHECK(!simd::all(int4{-1, -1, -1, 0}));
    CHECK_ALL((ivec<3>{-1, -1, -1}));
    CHECK(!simd::all(ivec<3>{0, -1, -1}));
    CHECK(!simd::all(ivec<3>{-1, 0, -1}));
    CHECK(!simd::all(ivec<3>{-1, -1, 0}));
    CHECK_ALL((int2{-1, -1}));
    CHECK(!simd::all(int2{0, -1}));
    CHECK(!simd::all(int2{-1, 0}));
    CHECK_ALL((ivec<1>{-1}));
    CHECK(!simd::all(ivec<1>{0}));
}

TEST_CASE("operators", "[simd]")
{
    float4 a{1, 2, 3, 4};
    float4 b{5, 6, 7, 8};
    CHECK_ALL((a + b == float4{6, 8, 10, 12}));
    CHECK_ALL((a - b == float4(-4)));
    CHECK_ALL((a * b == float4{5, 12, 21, 32}));
    CHECK_ALL((a / a == b / b));
    CHECK_ALL((a + 10.f == 10.f + a));
    CHECK_ALL((+(a - 10.f) == -(10.f - a)));
    CHECK_ALL((a * 2.f == 2.f * a));
    CHECK_ALL((a / .5f == 2.f * a));

    int4 i{1, 2, 4, 8};
    int4 j{0, 1, 3, 7};
    CHECK(!simd::any((i & j)));
    CHECK_ALL(((i | j) == int4{1, 3, 7, 15}));
    CHECK_ALL(((i | j) == (i ^ j)));
    CHECK_ALL((~i + 1 == -i)); // Assume two's compliment for now...
}

TEST_CASE("swizzles", "[simd]")
{
    float2 v2{1, -2};
    CHECK(v2.x == 1);
    CHECK(v2.y == -2);
    CHECK(v2.yx[0] == v2[1]);
    CHECK(v2.yx[1] == v2[0]);
    CHECK_ALL((float2(v2.yx) == float2{-2, 1}));
    CHECK_ALL((v2.yx == float2{-2, 1}));
    CHECK_ALL((v2.xyxy == float4{1, -2, 1, -2}));
    CHECK_ALL((v2.yxyx == float4{-2, 1, -2, 1}));

    float4 v4{1, -2, 3, -1};
    CHECK(v4.x == 1);
    CHECK(v4.y == -2);
    CHECK(v4.z == 3);
    CHECK(v4.w == -1);
    CHECK_ALL((v4.xy == float2{1, -2}));
    CHECK_ALL((v4.yz == float2{-2, 3}));
    CHECK_ALL((v4.zw == float2{3, -1}));
    CHECK_ALL((v4.xyz == vec<3>{1, -2, 3}));
    CHECK_ALL((v4.yzw == vec<3>{-2, 3, -1}));
    CHECK_ALL((v4.yxwz == float4{-2, 1, -1, 3}));
    CHECK_ALL((v4.zwxy == float4{3, -1, 1, -2}));
    CHECK_ALL((v4.zyxw == float4{3, -2, 1, -1}));
    CHECK_ALL((v4.xwzy == float4{1, -1, 3, -2}));

    v4.xy = v2.yx;
    CHECK_ALL((v4 == float4{-2, 1, 3, -1}));
    v4.zw = v4.yz;
    CHECK_ALL((v4 == float4{-2, 1, 1, 3}));
    v4.yz = -7.f;
    CHECK_ALL((v4 == float4{-2, -7, -7, 3}));
    v4.xyz = 0.f;
    CHECK_ALL((v4 == float4{0, 0, 0, 3}));
    v4.yzw = -9.f;
    CHECK_ALL((v4 == float4{0, -9, -9, -9}));
    v4.x = 1;
    v4.y = 2;
    v4.z = -8;
    v4.w = .5f;
    CHECK_ALL((v4 == float4{1, 2, -8, .5f}));

    v2.y = -9;
    v2.x = 88;
    CHECK_ALL((v2.yx == float2{-9, 88}));

    ivec<3> v3;
    v3.x = 0;
    v3.y = 9;
    v3.z = -1;
    CHECK_ALL((v3 == ivec<3>{0, 9, -1}));
    CHECK(v3.x == 0);
    CHECK(v3.y == 9);
    CHECK(v3.z == -1);

    uvec<1> v1;
    v1.x = 7;
    CHECK(v1[0] == 7);
    CHECK_ALL((v1 == uvec<1>{7}));

    float4 a = {0, 1, 12, 99.9f};
    float4 a_ = a.yxwz;
    float4 b = {.1f, -1, -9, -20};
    float4 b_ = b.yxwz;
    CHECK_ALL((simd::abs(b.yxwz) == simd::abs(b_)));
    CHECK_ALL((simd::floor(a.yxwz) == simd::floor(a_)));
    CHECK_ALL((simd::ceil(a.yxwz) == simd::ceil(a_)));
    CHECK_ALL((simd::sqrt(a.yxwz) == simd::sqrt(a_)));
    CHECK_ALL((simd::fast_acos(a.yxwz) == simd::fast_acos(a_)));
    CHECK_ALL((simd::min(a.yxwz, b.yxwz) == simd::min(a_, b_)));
    CHECK_ALL((simd::max(a.yxwz, b.yxwz) == simd::max(a_, b_)));
    CHECK_ALL(
        (simd::clamp(a.yxwz, float4(2), float4(10)) == simd::clamp(a_, float4(2), float4(10))));
    CHECK_ALL((simd::mix(a.yxwz, b.yxwz, float4(.5f)) == simd::mix(a_, b_, float4(.5f))));
    CHECK_ALL(
        (simd::precise_mix(a.yxwz, b.yxwz, float4(.5f)) == simd::precise_mix(a_, b_, float4(.5f))));
    CHECK_ALL(
        (simd::if_then_else(int4{~0}, a.yxwz, b.yxwz) == simd::if_then_else(int4{~0}, a_, b_)));
    CHECK_ALL((simd::if_then_else(int4{~0}, a.yxwz, b.yxwz) == float4{a.y, b.x, b.w, b.z}));
    CHECK_ALL((simd::if_then_else(~int4{~0}, a_, b_) == float4{b.y, a.x, a.w, a.z}));
    CHECK_ALL(
        (simd::if_then_else(int4{~0}, a.yxwz, b.yxwz) != simd::if_then_else(~int4{~0}, a_, b_)));

    float mem[4]{};
    simd::store(mem, a.yxwz);
    CHECK(!memcmp(mem, &a_, 4 * 4));

    // Unfortunate, but there's no way to block the default assignment operator and still be a POD
    // type.
    a.zwxy = b.zwxy;
    CHECK_ALL((a == b));
}

// Verify the simd float types are IEEE 754 compliant for infinity and NaN.
template <typename T> void check_ieee_compliance()
{
    using vec4 = simd::gvec<T, 4>;
    using vec2 = simd::gvec<T, 2>;
    constexpr T kTInf = std::numeric_limits<T>::infinity();

    vec4 test = vec4{1, -kTInf, 1, 4} / vec4{0, 2, kTInf, 4};
    CHECK_ALL((test == vec4{kTInf, -kTInf, 0, 1}));

    // Inf * Inf == Inf
    test = vec4{kTInf, -kTInf, kTInf, -kTInf} * vec4{kTInf, kTInf, -kTInf, -kTInf};
    CHECK_ALL((test == vec4{kTInf, -kTInf, -kTInf, kTInf}));

    // Inf/0 == Inf, 0/Inf == 0
    test = vec4{kTInf, -kTInf, 0, 0} / vec4{0, 0, kTInf, -kTInf};
    CHECK_ALL((test == vec4{kTInf, -kTInf, 0, 0}));

    // Inf/Inf, 0/0, 0 * Inf, Inf - Inf == NaN
    test = {kTInf, 0, 0, kTInf};
    test.xy /= vec2{kTInf, 0};
    test.z *= kTInf;
    test.w -= kTInf;
    for (int i = 0; i < 4; ++i)
    {
        CHECK(std::isnan(test[i]));
    }
    // NaN always fails comparisons.
    CHECK(!simd::any(test == test));
    CHECK_ALL((test != test));
    CHECK(!simd::any(test <= test));
    CHECK(!simd::any(test >= test));
    CHECK(!simd::any(test < test));
    CHECK(!simd::any(test > test));

    // Inf + Inf == Inf, Inf + -Inf == NaN
    test = vec4{kTInf, -kTInf, kTInf, -kTInf} + vec4{kTInf, -kTInf, -kTInf, kTInf};
    CHECK_ALL((test.xy == vec2{kTInf, -kTInf}));
    CHECK(!simd::any(test.zw == test.zw)); // NaN
}

TEST_CASE("ieee-compliance", "[simd]")
{
    check_ieee_compliance<float>();
    check_ieee_compliance<double>();
}

// Check simd::if_then_else.
template <typename T> void check_if_then_else()
{
    using vec4 = simd::gvec<T, 4>;
    using vec2 = simd::gvec<T, 2>;

    // Vector condition.
    vec4 f4 = simd::if_then_else(vec4{1, 2, 3, 4} < vec4{4, 3, 2, 1}, vec4(1), vec4(2));
    CHECK_ALL((f4 == vec4{1, 1, 2, 2}));

    // In vector, -1 is true, 0 is false.
    vec2 u2 = simd::if_then_else(simd::gvec<typename simd::boolean_mask_type<T>::type, 2>{0, -1},
                                 vec2{1, 2},
                                 vec2{3, 4});
    CHECK_ALL((u2 == vec2{3, 2}));

    // Scalar condition.
    f4 = u2.x == u2.y ? vec4{1, 2, 3, 4} : vec4{5, 6, 7, 8};
    CHECK_ALL((f4 == vec4{5, 6, 7, 8}));
}

TEST_CASE("ternary-operator", "[simd]")
{
    check_if_then_else<int8_t>();
    check_if_then_else<uint8_t>();
    check_if_then_else<int16_t>();
    check_if_then_else<uint16_t>();
    check_if_then_else<float>();
    check_if_then_else<int32_t>();
    check_if_then_else<uint32_t>();
    check_if_then_else<size_t>();
    check_if_then_else<double>();
    check_if_then_else<int64_t>();
    check_if_then_else<uint64_t>();
}

// Check simd::min/max compliance.
TEST_CASE("min-max", "[simd]")
{
    float4 f4 = simd::min(float4{1, 2, 3, 4}, float4{4, 3, 2});
    CHECK_ALL((f4 == float4{1, 2, 2, 0}));
    f4 = simd::max(float4{1, 2, 3, 4}, float4{4, 3, 2});
    CHECK_ALL((f4 == float4{4, 3, 3, 4}));

    int2 i2 = simd::max(int2(-1), int2{-2});
    CHECK_ALL((i2 == int2{-1, 0}));
    i2 = simd::min(int2(-1), int2{-2});
    CHECK_ALL((i2 == int2{-2, -1}));

    // Infinity works as expected.
    f4 = simd::min(float4{100, -kInf, -kInf, kInf}, float4{kInf, 100, kInf, -kInf});
    CHECK_ALL((f4 == float4{100, -kInf, -kInf, -kInf}));
    f4 = simd::max(float4{100, -kInf, -kInf, kInf}, float4{kInf, 100, kInf, -kInf});
    CHECK_ALL((f4 == float4{kInf, 100, kInf, kInf}));

    // If a or b is NaN, min returns whichever is not NaN.
    f4 = simd::min(float4{1, kNaN, 2, kNaN}, float4{kNaN, 1, 1, kNaN});
    CHECK_ALL((f4.xyz == 1.f));
    CHECK(std::isnan(f4.w));
    f4 = simd::max(float4{1, kNaN, 2, kNaN}, float4{kNaN, 1, 1, kNaN});
    CHECK_ALL((f4.xyz == vec<3>{1, 1, 2}));
    CHECK(std::isnan(f4.w));

    // simd::min/max differs from std::min/max when the first argument is NaN.
    CHECK(simd::min<float, 1>(kNaN, 1).x == 1);
    CHECK(std::isnan(std::min<float>(kNaN, 1)));
    CHECK(simd::max<float, 1>(kNaN, 1).x == 1);
    CHECK(std::isnan(std::max<float>(kNaN, 1)));
    CHECK(simd::min<double, 1>(kNaN, 1).x == 1);
    CHECK(std::isnan(std::min<double>(kNaN, 1)));
    CHECK(simd::max<double, 1>(kNaN, 1).x == 1);
    CHECK(std::isnan(std::max<double>(kNaN, 1)));

    // simd::min/max is equivalent std::min/max when the second argument is NaN.
    CHECK(simd::min<float, 1>(1, kNaN).x == std::min<float>(1, kNaN));
    CHECK(simd::max<float, 1>(1, kNaN).x == std::max<float>(1, kNaN));
    CHECK(simd::min<double, 1>(1, kNaN).x == std::min<double>(1, kNaN));
    CHECK(simd::max<double, 1>(1, kNaN).x == std::max<double>(1, kNaN));

    // check non-32-bit types.
    CHECK_ALL((simd::max(simd::gvec<double, 2>{3, 4}, simd::gvec<double, 2>{4, 3}) ==
               simd::gvec<double, 2>{4, 4}));
    CHECK_ALL((simd::min(simd::gvec<uint64_t, 2>{3, 4}, simd::gvec<uint64_t, 2>{4, 3}) ==
               simd::gvec<uint64_t, 2>{3, 3}));
    CHECK_ALL((simd::max(simd::gvec<size_t, 2>{3, 4}, simd::gvec<size_t, 2>{4, 3}) ==
               simd::gvec<size_t, 2>{4, 4}));
    CHECK_ALL(
        (simd::max(simd::gvec<uint8_t, 16>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                   simd::gvec<uint8_t, 16>{15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0}) ==
         simd::gvec<uint8_t, 16>{15, 14, 13, 12, 11, 10, 9, 8, 8, 9, 10, 11, 12, 13, 14, 15}));
}

// Check simd::clamp.
TEST_CASE("clamp", "[simd]")
{
    CHECK_ALL(
        (simd::clamp(float4{1, 2, kInf, -kInf}, float4{2, 1, kInf, 0}, float4{3, 1, kInf, kInf}) ==
         float4{2, 1, kInf, 0}));
    CHECK_ALL((simd::clamp(float4{1, kNaN, kInf, -kInf},
                           float4{kNaN, 2, kNaN, 0},
                           float4{kNaN, 3, kInf, kNaN}) == float4{1, 2, kInf, 0}));
    float4 f4 = simd::clamp(float4{1, kNaN, kNaN, kNaN},
                            float4{kNaN, 1, kNaN, kNaN},
                            float4{kNaN, kNaN, 1, kNaN});
    CHECK_ALL((1.f == f4.xyz));
    CHECK(std::isnan(f4.w));

    // Returns lo if x == NaN, but std::clamp() returns NaN.
    CHECK(simd::clamp<float, 1>(kNaN, 1, 2).x == 1);

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
    CHECK_ALL((simd::abs(float4{-1, 2, -3, 4}) == float4{1, 2, 3, 4}));
    CHECK_ALL((simd::abs(float2{-5, 6}) == float2{5, 6}));
    CHECK_ALL((simd::abs(float2{-0, 0}) == float2{0, 0}));
    CHECK_ALL((float4{-std::numeric_limits<float>::epsilon(),
                      -std::numeric_limits<float>::denorm_min(),
                      -std::numeric_limits<float>::max(),
                      -kInf} == float4{-std::numeric_limits<float>::epsilon(),
                                       -std::numeric_limits<float>::denorm_min(),
                                       -std::numeric_limits<float>::max(),
                                       -kInf}

               ));
    float2 nan2 = simd::abs(float2{kNaN, -kNaN});
    CHECK_ALL((simd::isnan(nan2)));
    CHECK_ALL((simd::abs(int4{7, -8, 9, -10}) == int4{7, 8, 9, 10}));
    CHECK_ALL((simd::abs(int2{0, -0}) == int2{0, 0}));
    // abs(INT_MIN) returns INT_MIN.
    CHECK(
        simd::all(simd::abs(int2{-std::numeric_limits<int32_t>::max(),
                                 std::numeric_limits<int32_t>::min()}) ==
                  int2{std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min()}));
}

// Check simd::reduce* methods.
TEST_CASE("reduce", "[simd]")
{
    {
        float4 v = {1, 2, 3, 4};
        CHECK(simd::reduce_add(v) == 10);
        CHECK(simd::reduce_add(v.zwxy) == 10);
        CHECK(simd::reduce_add(v.xyz) == 6);
        CHECK(simd::reduce_add(v.yz) == 5);
        CHECK(simd::reduce_add(v.xy.yxyx) == 6);
        CHECK(simd::reduce_min(v) == 1);
        CHECK(simd::reduce_min(v.zwxy) == 1);
        CHECK(simd::reduce_min(v.xyz) == 1);
        CHECK(simd::reduce_min(v.yz) == 2);
        CHECK(simd::reduce_min(v.xy.yxyx) == 1);
        CHECK(simd::reduce_max(v) == 4);
        CHECK(simd::reduce_max(v.zwxy) == 4);
        CHECK(simd::reduce_max(v.xyz) == 3);
        CHECK(simd::reduce_max(v.yz) == 3);
        CHECK(simd::reduce_max(v.xy.yxyx) == 2);
    }

    {
        int4 v = {1, 2, 3, 4};
        CHECK(simd::reduce_add(v) == 10);
        CHECK(simd::reduce_add(v.zwxy) == 10);
        CHECK(simd::reduce_add(v.xyz) == 6);
        CHECK(simd::reduce_add(v.yz) == 5);
        CHECK(simd::reduce_add(v.xy.yxyx) == 6);
        CHECK(simd::reduce_min(v) == 1);
        CHECK(simd::reduce_min(v.zwxy) == 1);
        CHECK(simd::reduce_min(v.xyz) == 1);
        CHECK(simd::reduce_min(v.yz) == 2);
        CHECK(simd::reduce_min(v.xy.yxyx) == 1);
        CHECK(simd::reduce_max(v) == 4);
        CHECK(simd::reduce_max(v.zwxy) == 4);
        CHECK(simd::reduce_max(v.xyz) == 3);
        CHECK(simd::reduce_max(v.yz) == 3);
        CHECK(simd::reduce_max(v.xy.yxyx) == 2);
        CHECK(simd::reduce_and(v) == 0);
        CHECK(simd::reduce_and(v.zwxy) == 0);
        CHECK(simd::reduce_and(v.xyz) == 0);
        CHECK(simd::reduce_and(v.yz) == 2);
        CHECK(simd::reduce_and(v.xy.yxyx) == 0);
        CHECK(simd::reduce_or(v) == 7);
        CHECK(simd::reduce_or(v.zwxy) == 7);
        CHECK(simd::reduce_or(v.xyz) == 3);
        CHECK(simd::reduce_or(v.yz) == 3);
        CHECK(simd::reduce_or(v.xy.yxyx) == 3);
    }

    {
        uint4 v = {1, 2, 3, 4};
        CHECK(simd::reduce_add(v) == 10);
        CHECK(simd::reduce_add(v.zwxy) == 10);
        CHECK(simd::reduce_add(v.xyz) == 6);
        CHECK(simd::reduce_add(v.yz) == 5);
        CHECK(simd::reduce_add(v.xy.yxyx) == 6);
        CHECK(simd::reduce_min(v) == 1);
        CHECK(simd::reduce_min(v.zwxy) == 1);
        CHECK(simd::reduce_min(v.xyz) == 1);
        CHECK(simd::reduce_min(v.yz) == 2);
        CHECK(simd::reduce_min(v.xy.yxyx) == 1);
        CHECK(simd::reduce_max(v) == 4);
        CHECK(simd::reduce_max(v.zwxy) == 4);
        CHECK(simd::reduce_max(v.xyz) == 3);
        CHECK(simd::reduce_max(v.yz) == 3);
        CHECK(simd::reduce_max(v.xy.yxyx) == 2);
        CHECK(simd::reduce_and(v) == 0);
        CHECK(simd::reduce_and(v.zwxy) == 0);
        CHECK(simd::reduce_and(v.xyz) == 0);
        CHECK(simd::reduce_and(v.yz) == 2);
        CHECK(simd::reduce_and(v.xy.yxyx) == 0);
        CHECK(simd::reduce_or(v) == 7);
        CHECK(simd::reduce_or(v.zwxy) == 7);
        CHECK(simd::reduce_or(v.xyz) == 3);
        CHECK(simd::reduce_or(v.yz) == 3);
        CHECK(simd::reduce_or(v.xy.yxyx) == 3);
    }
}

// Check simd::floor.
TEST_CASE("floor", "[simd]")
{
    CHECK_ALL((simd::floor(float4{-1.9f, 1.9f, 2, -2}) == float4{-2, 1, 2, -2}));
    CHECK_ALL((simd::floor(float2{kInf, -kInf}) == float2{kInf, -kInf}));
    CHECK_ALL((simd::isnan(simd::floor(float2{kNaN, -kNaN}))));
}

// Check simd::ceil.
TEST_CASE("ceil", "[simd]")
{
    CHECK_ALL((simd::ceil(float4{-1.9f, 1.9f, 2, -2}) == float4{-1, 2, 2, -2}));
    CHECK_ALL((simd::ceil(float2{kInf, -kInf}) == float2{kInf, -kInf}));
    CHECK_ALL((simd::isnan(simd::ceil(float2{kNaN, -kNaN}))));
}

// Check simd::sqrt.
TEST_CASE("sqrt", "[simd]")
{
    CHECK_ALL((simd::sqrt(float4{1, 4, 9, 16}) == float4{1, 2, 3, 4}));
    CHECK_ALL((simd::sqrt(float2{25, 36}) == float2{5, 6}));
    CHECK_ALL((simd::sqrt(vec<1>{36}) == vec<1>{6}));
    CHECK_ALL((simd::sqrt(vec<5>{49, 64, 81, 100, 121}) == vec<5>{7, 8, 9, 10, 11}));
    CHECK_ALL((simd::isnan(simd::sqrt(float4{-1, -kInf, kNaN, -2}))));
    CHECK_ALL((simd::sqrt(vec<3>{kInf, 0, 1}) == vec<3>{kInf, 0, 1}));
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
        float8 f_ = 3.f * b * xx + a;
        float8 g = (d * xx + c) * xx + 1.f;
        float8 g_ = (4.f * d * xx + 2.f * c) * x;
        float8 gg = g * g;
        float8 q = simd::sqrt(1.f - xx);
        err_ = (f_ * g - f * g_) / gg + 1.f / q;

        // Find d^2/dx^2(error)
        //    = ((f''g - fg'')g^2 - (f'g - fg')2gg') / g^4 + x(1 - x^2)^(-3/2)
        //    = ((f''g - fg'')g - (f'g - fg')2g') / g^3 + x(1 - x^2)^(-3/2)
        float8 f__ = 6.f * b * x;
        float8 g__ = 12.f * d * xx + 2.f * c;
        float8 err__ = ((f__ * g - f * g__) * g - (f_ * g - f * g_) * 2.f * g_) / (gg * g) +
                       x / ((1.f - xx) * q);

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
        CHECK_ANY((simd::abs(x - knownRoot) < math::EPSILON));
    }
}

TEST_CASE("cast", "[simd]")
{
    float4 f4 = float4{-1.9f, -1.5f, 1.5f, 1.1f};
    CHECK(simd::all(simd::cast<int>(f4) == int4{-1, -1, 1, 1}));
    CHECK(simd::all(simd::cast<int>(simd::floor(f4)) == int4{-2, -2, 1, 1}));
    CHECK(simd::all(simd::cast<int>(simd::ceil(f4)) == int4{-1, -1, 2, 2}));
    CHECK(simd::all(simd::cast<int>(simd::ceil(f4.zwxy)) == int4{2, 2, -1, -1}));
    CHECK(simd::all(simd::cast<int>(simd::ceil(f4).zwxy) == int4{2, 2, -1, -1}));
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
    CHECK_ALL((simd::join(int2{1, 2}, int4{3, 4, 5, 6}) == ivec<6>{1, 2, 3, 4, 5, 6}));
    CHECK_ALL((simd::join(vec<1>{1}, vec<3>{2, 3, 4}) == float4{1, 2, 3, 4}));
    CHECK_ALL((simd::join(vec<1>{1}, vec<2>{2, 3}, vec<3>{4, 5, 6}) == vec<6>{1, 2, 3, 4, 5, 6}));
    CHECK_ALL((simd::join(vec<1>{1}, vec<2>{2, 3}, vec<3>{4, 5, 6}, float4{7, 8, 9, 10}) ==
               vec<10>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
    uint8x8 a = 3, b = 9, c = 3, d = 100;
    CHECK_ALL((simd::join(a, b, c, d) == uint8x32{3, 3, 3,   3,   3,   3,   3,   3,   9,   9,  9,
                                                  9, 9, 9,   9,   9,   3,   3,   3,   3,   3,  3,
                                                  3, 3, 100, 100, 100, 100, 100, 100, 100, 100}));
}

// Check simd::zip
TEST_CASE("zip", "[simd]")
{
    CHECK_ALL((simd::zip(simd::gvec<char, 1>{'a'}, simd::gvec<char, 1>{'b'}) ==
               simd::gvec<char, 2>{'a', 'b'}));
    CHECK_ALL((simd::zip(int2{1, 2}, int2{3, 4}) == int4{1, 3, 2, 4}));
    CHECK_ALL((simd::zip(int4{1, 2, 3, 4}, int4{5, 6, 7, 8}) == ivec<8>{1, 5, 2, 6, 3, 7, 4, 8}));
    CHECK_ALL((simd::zip(simd::gvec<uint8_t, 8>{1, 2, 3, 4, 5, 6, 7, 8},
                         simd::gvec<uint8_t, 8>{9, 10, 11, 12, 13, 14, 15, 16}) ==
               simd::gvec<uint8_t, 16>{1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15, 8, 16}));
    CHECK_ALL(
        (simd::zip(float4{1, 2, 3, 4}, float4{5, 6, 7, 8}) == vec<8>{1, 5, 2, 6, 3, 7, 4, 8}));
}

template <int N> static vec<N> mix_reference_impl(vec<N> a, vec<N> b, float t)
{
    return a * (1.f - t) + b * t;
}
template <int N> static vec<N> mix_reference_impl(vec<N> a, vec<N> b, vec<N> t)
{
    return a * (1.f - t) + b * t;
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
    CHECK(fuzzy_equal(simd::mix(a, b, vec<N>(t)), mix_reference_impl(a, b, t)));
    CHECK(fuzzy_equal(simd::precise_mix(a, b, vec<N>(t)), mix_reference_impl(a, b, t)));
    vec<N> tt = vrand<N>();
    CHECK(fuzzy_equal(simd::mix(a, b, tt), mix_reference_impl(a, b, tt)));
    CHECK(fuzzy_equal(simd::precise_mix(a, b, tt), mix_reference_impl(a, b, tt)));
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
    CHECK_ALL((simd::mix(float4{1, 2, 3, 4}, float4{5, 6, 7, 8}, float4(0)) == float4{1, 2, 3, 4}));
    CHECK_ALL((simd::precise_mix(float4{-1, 2, 3, 4}, float4{5, 6, 7, 8}, float4(0)) ==
               float4{-1, 2, 3, 4}));
    CHECK_ALL((simd::precise_mix(float4{1, 2, 3, 4}, float4{5, -6, 7, -8}, float4(1)) ==
               float4{5, -6, 7, -8}));
}

// Check simd::load4x4f
TEST_CASE("load4x4f", "[simd]")
{
    // Column major.
    float m[16] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
    auto c = simd::load4x4f(m);
    CHECK(simd::all(std::get<0>(c) == float4{0, 1, 2, 3}));
    CHECK(simd::all(std::get<1>(c) == float4{4, 5, 6, 7}));
    CHECK(simd::all(std::get<2>(c) == float4{8, 9, 10, 11}));
    CHECK(simd::all(std::get<3>(c) == float4{12, 13, 14, 15}));
}

} // namespace rive
