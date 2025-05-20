/*
 * Copyright 2025 Rive
 */

#include "catch.hpp"

#include "rive/object_stream.hpp"
#include "rive/math/simd.hpp"
#include <array>
#include <numeric>

using namespace rive;

TEST_CASE("ObjectStream", "[ObjectStream]")
{
    ObjectStream<std::string> s;
    CHECK(s.empty());
    s << "hello";
    CHECK(!s.empty());
    s << "world"
      << "hi";
    CHECK(!s.empty());
    s << "rive too";
    CHECK(!s.empty());

    std::string str, str2, str3;
    s >> str >> str2 >> str3;
    CHECK(str == "hello");
    CHECK(str2 == "world");
    CHECK(str3 == "hi");
    CHECK(!s.empty());
    s >> str;
    CHECK(str == "rive too");
    CHECK(s.empty());
}

TEST_CASE("ObjectStream -- force deque realloc", "[ObjectStream]")
{
    ObjectStream<std::vector<int>> s;
    for (int i = 0; i < 100; ++i)
    {
        std::vector<int> v(i);
        std::iota(v.begin(), v.end(), i * 123 + 257);
        s << v;
    }
    for (int i = 0; i < 100; ++i)
    {
        std::vector<int> v(i), v2;
        std::iota(v.begin(), v.end(), i * 123 + 257);
        s >> v2;
        CHECK(v2.size() == i);
        CHECK(v == v2);
    }
    CHECK(s.empty());
}

TEST_CASE("PODStream", "[ObjectStream]")
{
    PODStream s;
    CHECK(s.empty());

    struct Pair
    {
        int x;
        float y;
        inline bool operator==(const Pair& rhs) const
        {
            return x == rhs.x && y == rhs.y;
        }
    };
    Pair pair = {1, 3.f};

    bool boolean = true;
    vec<3> f3d = {3, 2, 1};

    s << pair;
    CHECK(!s.empty());
    s << boolean << f3d;
    CHECK(!s.empty());

    Pair pair2 = {};
    bool boolean2 = {};
    vec<3> f3d2 = {};
    float4 f4d2 = {};
    std::array<int, 2> array2 = {};

    CHECK(!s.empty());
    s >> pair2;
    CHECK(pair == pair2);

    CHECK(!s.empty());
    s << float4{1, 2, 3, 4} << std::array<int, 2>{1, 2};

    CHECK(!s.empty());
    s >> boolean2;
    CHECK(boolean == boolean2);

    CHECK(!s.empty());
    s >> f3d2 >> f4d2;
    CHECK(simd::all(f3d == f3d2));
    CHECK(simd::all(f4d2 == float4{1, 2, 3, 4}));

    CHECK(!s.empty());
    s >> array2;
    CHECK(array2 == std::array<int, 2>{1, 2});

    CHECK(s.empty());
}

TEST_CASE("PODStream -- force deque realloc", "[ObjectStream]")
{
    PODStream s;
    for (int16_t i16 = 0; i16 < 1 << 12; ++i16)
    {
        int8_t i8 = static_cast<int8_t>(i16 - 3);
        int32_t i32 = i16 + 12;
        int64_t i64 = i32 << 16;
        int2 i32x2 = {15, i32};
        int4 i32x4 = {17, 18, i32, 20};
        s << true << false << i8 << i16 << i32 << i64 << i32x2 << i32x4;
        s << i32x4 << false << i32 << i32x2 << true << i64 << i8 << i16;
    }
    for (int16_t i = 0; i < 1 << 12; ++i)
    {
        bool b1, b2;
        int8_t i8;
        int16_t i16;
        int32_t i32;
        int64_t i64;
        int2 i32x2;
        int4 i32x4;
        s >> b1 >> b2 >> i8 >> i16 >> i32 >> i64 >> i32x2 >> i32x4;
        CHECK(b1 == true);
        CHECK(b2 == false);
        CHECK(i8 == static_cast<int8_t>(i16 - 3));
        CHECK(i16 == i);
        CHECK(i32 == i16 + 12);
        CHECK(i64 == i32 << 16);
        CHECK(simd::all(i32x2 == int2{15, i32}));
        CHECK(simd::all(i32x4 == int4{17, 18, i32, 20}));
        s >> i32x4 >> b1 >> i32 >> i32x2 >> b2 >> i64 >> i8 >> i16;
        CHECK(b1 == false);
        CHECK(b2 == true);
        CHECK(i8 == static_cast<int8_t>(i16 - 3));
        CHECK(i16 == i);
        CHECK(i32 == i16 + 12);
        CHECK(i64 == i32 << 16);
        CHECK(simd::all(i32x2 == int2{15, i32}));
        CHECK(simd::all(i32x4 == int4{17, 18, i32, 20}));
    }
    CHECK(s.empty());
}
