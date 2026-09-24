/*
 * Copyright 2026 Rive
 */

// Exhaustive validation of packNormalizedDepth() from common.glsl, compiled as
// C++. depthStencil mode packs a path's z index and coverage into a depth
// buffer and relies on the value landing on an exact integer once the hardware
// scales it back up, so the interesting property isn't "close enough" -- it's
// that every one of the 2^23 payloads round-trips exactly, for both the 0..1
// and the -1..+1 convention.

#include "common/rand.hpp"
#include <catch.hpp>

// <array> is included in "cpp.glsl", so include it here first to keep its
// contents out of the glsl_cross namespaces below.
#include <array>
#include <cmath>
#include <cstring>

#define RIVE_GLSL_CROSS_WANT_CONSTRUCTORS
#include "cpp.glsl"

// packNormalizedDepth() lives in common.glsl's vertex-side depthStencil
// section, so select that the way a real build would.
#define VERTEX
#define RENDER_MODE_DEPTH_STENCIL

// The two depth conventions are separate builds of the same source, so each
// gets its own namespace and the test can check both in one run. Both define
// GLSL, whose cast helpers are the ones that compile as C++; what separates
// them is TARGET_SPIRV, which is also what selects the depth convention.
namespace vk_path
{
RIVE_GLSL_CROSS_CONSTRUCTORS
#define GLSL
#define TARGET_SPIRV // Selects the 0..1 depth convention.
#include "generated/shaders/constants.minified.glsl"
#include "generated/shaders/common.minified.glsl"
#undef TARGET_SPIRV
#undef GLSL
} // namespace vk_path

namespace gl_path
{
RIVE_GLSL_CROSS_CONSTRUCTORS
#define GLSL // Without TARGET_SPIRV: the -1..+1 depth convention.
#include "generated/shaders/common.minified.glsl"
#undef GLSL
} // namespace gl_path

// The payload is a z index above a coverage field; see constants.glsl.
constexpr static uint32_t PayloadBitCount =
    DEPTH_Z_INDEX_BIT_COUNT + DEPTH_COVERAGE_BIT_COUNT;
static_assert(PayloadBitCount == 23);
constexpr static uint32_t PayloadCount = 1u << PayloadBitCount;
constexpr static uint32_t ZIndexCount = 1u << DEPTH_Z_INDEX_BIT_COUNT;
constexpr static uint32_t CoverageMask = (1u << DEPTH_COVERAGE_BIT_COUNT) - 1;

// What a D24_UNORM buffer stores for a given depth value.
static uint32_t hardwareStores(float depth)
{
    return static_cast<uint32_t>(std::lround(depth * 0xffffff));
}

TEST_CASE("packNormalizedDepth-exhaustive-0-to-1", "[pack_normalized_depth]")
{
    // Every payload has to survive the trip through a 24-bit depth buffer: the
    // shader's value, scaled by 0xffffff and rounded the way the hardware
    // does, must come back as exactly the payload we put in.
    float prev = -1.f;
    for (uint32_t payload = 0; payload < PayloadCount; ++payload)
    {
        const float depth =
            vk_path::packNormalizedDepth(payload >> DEPTH_COVERAGE_BIT_COUNT,
                                         payload & CoverageMask);

        REQUIRE(depth > 0.f);
        // The payload never reaches the depth buffer's 24th bit.
        REQUIRE(depth < .5f);
        REQUIRE(hardwareStores(depth) == payload);
        // The multiply-add the shader emits is bit-for-bit an exact
        // (payload + .5) * 2^-24, which is why it doesn't need an ldexp().
        const float d = static_cast<float>(payload);
        REQUIRE(depth == (d + .5f) * (1.f / 16777216.f));
        REQUIRE(depth == std::ldexp(d + .5f, -24));
        // Strictly increasing, so the depth test orders by z index first and
        // by coverage second.
        REQUIRE(depth > prev);
        prev = depth;
    }
}

TEST_CASE("packNormalizedDepth-exhaustive-minus1-to-1",
          "[pack_normalized_depth]")
{
    float prev = -2.f;
    for (uint32_t payload = 0; payload < PayloadCount; ++payload)
    {
        const float depth =
            gl_path::packNormalizedDepth(payload >> DEPTH_COVERAGE_BIT_COUNT,
                                         payload & CoverageMask);

        REQUIRE(depth > -1.f);
        // The payload never reaches the depth buffer's 24th bit, which here
        // shows up as the value staying in the lower half: -1..0.
        REQUIRE(depth < 0.f);
        // GL's -1..+1 maps to the same fixed-point slot after the viewport
        // transform halves it and shifts it up.
        REQUIRE(hardwareStores((depth + 1.f) * .5f) == payload);
        const float d = static_cast<float>(payload);
        REQUIRE(depth == (d + .5f) * (1.f / 8388608.f) - 1.f);
        REQUIRE(depth == std::ldexp(d + .5f, -23) - 1.f);
        REQUIRE(depth > prev);
        prev = depth;
    }
}

TEST_CASE("packNormalizedDepth-scale-and-bias", "[pack_normalized_depth]")
{
    // The shader assembles its scale and bias from bit patterns rather than
    // trusting a compiler to fold "1. / 16777216." exactly. These are the
    // values those patterns have to be.
    CHECK(uintBitsToFloat(0x33800000u) == 1.f / 16777216.f); // 2^-24
    CHECK(uintBitsToFloat(0x33000000u) == .5f / 16777216.f); // 2^-25
    CHECK(uintBitsToFloat(0x34000000u) == 1.f / 8388608.f);  // 2^-23
    CHECK(uintBitsToFloat(0xbf7fffffu) == .5f / 8388608.f - 1.f);
}

TEST_CASE("packNormalizedDepth-24-bits-does-not-pack",
          "[pack_normalized_depth]")
{
    // The cap is 23 bits, not 24, and this is why: one bit higher, D + .5 sits
    // exactly halfway between two floats, so it rounds to an integer and the
    // half-step -- the whole reason the round-trip survives the hardware's
    // "round(z * 0xffffff)" -- disappears. Exhaustive over the 24-bit range so
    // nobody widens the payload on the theory that f32 holds 24 bits (it does;
    // D + .5 doesn't).
    uint32_t halfSurvived = 0;
    uint32_t roundTripped = 0;
    for (uint32_t payload = 1u << 23; payload < 1u << 24; ++payload)
    {
        const float d = static_cast<float>(payload);
        // d itself is still exact up here...
        REQUIRE(static_cast<uint32_t>(d) == payload);
        // ...but d + .5 is not, and ties-to-even lands it on an integer.
        const float biased = d + .5f;
        if (static_cast<double>(biased) == static_cast<double>(payload) + 0.5)
        {
            ++halfSurvived;
        }
        REQUIRE(biased == std::floor(biased));
        if (hardwareStores(biased * (1.f / 16777216.f)) == payload)
        {
            ++roundTripped;
        }
    }
    CHECK(halfSurvived == 0);
    // Half of them land back on the wrong integer.
    CHECK(roundTripped < (1u << 23));
    CHECK(roundTripped > 0); // The other half survive by luck, not by design.

    // The last payload that does work is the one the cap allows.
    const uint32_t lastGood = PayloadCount - 1;
    const float d = static_cast<float>(lastGood);
    CHECK(static_cast<double>(d + .5f) ==
          static_cast<double>(lastGood) + 0.5); // Half-step intact.
    CHECK(hardwareStores((d + .5f) * (1.f / 16777216.f)) == lastGood);
}

TEST_CASE("packNormalizedDepth-24th-bit-unused", "[pack_normalized_depth]")
{
    // The payload cap leaves the depth buffer's top bit clear. Anyone tempted
    // to reclaim it should see this fail.
    const float maxDepth =
        vk_path::packNormalizedDepth(ZIndexCount - 1, CoverageMask);
    CHECK(maxDepth < .5f);
    CHECK(hardwareStores(maxDepth) == PayloadCount - 1);
}
