/*
 * Copyright 2026 Rive
 */

#include <rive/core/type_conversions.hpp>
#include <catch.hpp>
#include <cstdint>
#include <limits>

using namespace rive;

TEST_CASE("checkedMul accepts zero inputs", "[type_conversions]")
{
    size_t out = 0xdeadbeef;
    REQUIRE(checkedMul<size_t>(0, 0, &out));
    REQUIRE(out == 0);

    REQUIRE(checkedMul<size_t>(0, std::numeric_limits<size_t>::max(), &out));
    REQUIRE(out == 0);

    REQUIRE(checkedMul<size_t>(std::numeric_limits<size_t>::max(), 0, &out));
    REQUIRE(out == 0);
}

TEST_CASE("checkedMul preserves identity", "[type_conversions]")
{
    uint32_t out32;
    REQUIRE(checkedMul<uint32_t>(1, 12345, &out32));
    REQUIRE(out32 == 12345);
    REQUIRE(checkedMul<uint32_t>(67890, 1, &out32));
    REQUIRE(out32 == 67890);

    uint64_t out64;
    REQUIRE(
        checkedMul<uint64_t>(1, std::numeric_limits<uint64_t>::max(), &out64));
    REQUIRE(out64 == std::numeric_limits<uint64_t>::max());
}

TEST_CASE("checkedMul computes small products", "[type_conversions]")
{
    size_t out;
    REQUIRE(checkedMul<size_t>(2, 3, &out));
    REQUIRE(out == 6);
    REQUIRE(checkedMul<size_t>(7, 11, &out));
    REQUIRE(out == 77);
}

TEST_CASE("checkedMul succeeds just below overflow", "[type_conversions]")
{
    {
        constexpr uint32_t max32 = std::numeric_limits<uint32_t>::max();
        uint32_t out;
        REQUIRE(checkedMul<uint32_t>(max32 / 2, 2, &out));
        REQUIRE(out == (max32 / 2) * 2);
    }
    {
        constexpr uint64_t max64 = std::numeric_limits<uint64_t>::max();
        uint64_t out;
        REQUIRE(checkedMul<uint64_t>(max64 / 2, 2, &out));
        REQUIRE(out == (max64 / 2) * 2);
    }
}

TEST_CASE("checkedMul detects overflow at boundary", "[type_conversions]")
{
    {
        constexpr uint32_t max32 = std::numeric_limits<uint32_t>::max();
        uint32_t out;
        REQUIRE(!checkedMul<uint32_t>(max32 / 2 + 1, 2, &out));
    }
    {
        constexpr uint64_t max64 = std::numeric_limits<uint64_t>::max();
        uint64_t out;
        REQUIRE(!checkedMul<uint64_t>(max64 / 2 + 1, 2, &out));
    }
}

TEST_CASE("checkedMul detects square overflow", "[type_conversions]")
{
    constexpr uint32_t max32 = std::numeric_limits<uint32_t>::max();
    uint32_t out32;
    REQUIRE(!checkedMul<uint32_t>(max32, max32, &out32));

    constexpr uint64_t max64 = std::numeric_limits<uint64_t>::max();
    uint64_t out64;
    REQUIRE(!checkedMul<uint64_t>(max64, max64, &out64));
}

TEST_CASE("checkedMul covers narrow unsigned widths", "[type_conversions]")
{
    {
        uint8_t out;
        REQUIRE(checkedMul<uint8_t>(15, 17, &out));
        REQUIRE(out == 255);
        REQUIRE(!checkedMul<uint8_t>(16, 16, &out));
    }
    {
        uint16_t out;
        REQUIRE(checkedMul<uint16_t>(255, 257, &out));
        REQUIRE(out == 65535);
        REQUIRE(!checkedMul<uint16_t>(256, 257, &out));
    }
}

TEST_CASE("checkedMul covers size_t directly", "[type_conversions]")
{
    // size_t aliases uint64_t on 64-bit and uint32_t on 32-bit (e.g. wasm).
    // Asserting on size_t directly locks the API surface used by the wave-2
    // call-sites (image decoders, manifest asset, SimpleArray).
    constexpr size_t maxSz = std::numeric_limits<size_t>::max();
    size_t out;
    REQUIRE(checkedMul<size_t>(0, maxSz, &out));
    REQUIRE(out == 0);
    REQUIRE(checkedMul<size_t>(7, 11, &out));
    REQUIRE(out == 77);
    REQUIRE(checkedMul<size_t>(maxSz / 2, 2, &out));
    REQUIRE(out == (maxSz / 2) * 2);
    REQUIRE(!checkedMul<size_t>(maxSz / 2 + 1, 2, &out));
    REQUIRE(!checkedMul<size_t>(maxSz, maxSz, &out));
}

TEST_CASE("checkedMul tolerates output aliasing input", "[type_conversions]")
{
    // Both the __builtin_mul_overflow path and the manual fallback must be
    // safe when *out aliases one of the operands. Locks the contract so a
    // future fallback refactor cannot quietly diverge.
    {
        size_t x = 7;
        REQUIRE(checkedMul<size_t>(x, 11, &x));
        REQUIRE(x == 77);
    }
    {
        size_t y = 13;
        REQUIRE(checkedMul<size_t>(3, y, &y));
        REQUIRE(y == 39);
    }
    {
        constexpr size_t maxSz = std::numeric_limits<size_t>::max();
        size_t z = maxSz;
        REQUIRE(!checkedMul<size_t>(z, 2, &z));
        // On overflow, *out is allowed to hold the wrapped low bits per the
        // documented contract — we don't assert a specific value, just that
        // the call returned false and didn't crash.
    }
}

TEST_CASE("mulOverflows inverts checkedMul", "[type_conversions]")
{
    constexpr size_t maxSz = std::numeric_limits<size_t>::max();
    size_t dummy;

    REQUIRE(mulOverflows<size_t>(maxSz, 2) ==
            !checkedMul<size_t>(maxSz, 2, &dummy));
    REQUIRE(mulOverflows<size_t>(7, 11) == !checkedMul<size_t>(7, 11, &dummy));
    REQUIRE(mulOverflows<size_t>(0, maxSz) ==
            !checkedMul<size_t>(0, maxSz, &dummy));
}

TEST_CASE("mulOverflows captures decoder-shaped sizing", "[type_conversions]")
{
    // 65537 * 65537 = 4'295'098'369 — fits in size_t on 64-bit, overflows
    // uint32_t. This is the same value class used by the wave-2 image-decoder
    // fixes when computing pixel-buffer byte counts.
    REQUIRE(!mulOverflows<size_t>(65537, 65537));
    REQUIRE(mulOverflows<uint32_t>(65537, 65537));
}
