/*
 * Copyright 2026 Rive
 */

// Silver is the portable field wise form with a GPU free comparator, the
// cross arch regression form that the host endian serialize is not.

#include "rive/renderer/ore/cmd/ore_command_silver.hpp"

#include <catch.hpp>
#include <vector>

using namespace rive::ore;
using namespace rive::ore::cmd;

// Covers every command type, including blob arena dynamic offsets and a
// negative baseVertex.
static void recordRepresentative(OreCommandBuffer& buf)
{
    BeginRenderPassCmd begin{};
    begin.colorCount = 2;
    begin.colors[0] = {0,
                       kInvalidHandle,
                       LoadOp::clear,
                       StoreOp::store,
                       0.1f,
                       0.2f,
                       0.3f,
                       1.0f};
    begin.colors[1] =
        {1, 2, LoadOp::load, StoreOp::discard, 0.0f, 0.0f, 0.0f, 0.0f};
    begin.depthStencil = {3,
                          LoadOp::clear,
                          StoreOp::store,
                          1.0f,
                          LoadOp::clear,
                          StoreOp::store,
                          0};
    buf.append(CommandType::beginRenderPass, begin);

    buf.append(CommandType::setPipeline, SetPipelineCmd{7});
    buf.append(CommandType::setVertexBuffer, SetVertexBufferCmd{0, 4, 16});
    buf.append(CommandType::setIndexBuffer,
               SetIndexBufferCmd{5, IndexFormat::uint16, 0});

    const uint32_t dynOffsets[2] = {64, 128};
    uint64_t dynStart = buf.appendBlob(dynOffsets, sizeof(dynOffsets));
    buf.append(CommandType::setBindGroup, SetBindGroupCmd{1, 6, dynStart, 2});

    buf.append(CommandType::setViewport,
               SetViewportCmd{0.f, 0.f, 256.f, 128.f, 0.f, 1.f});
    buf.append(CommandType::setScissorRect, SetScissorRectCmd{0, 0, 256, 128});
    buf.append(CommandType::setStencilReference, SetStencilReferenceCmd{0x80});
    buf.append(CommandType::setBlendColor,
               SetBlendColorCmd{1.f, 0.5f, 0.f, 1.f});
    buf.append(CommandType::draw, DrawCmd{6, 2, 1, 0});
    buf.append(CommandType::drawIndexed, DrawIndexedCmd{12, 1, 0, -3, 0});
    buf.appendOpcode(CommandType::finish);
}

TEST_CASE("ore silver round-trips and self-compares equal", "[ore][cmd]")
{
    OreCommandBuffer buf;
    recordRepresentative(buf);

    std::vector<uint8_t> silver;
    serializeSilver(buf, silver);
    REQUIRE(silver.size() > sizeof(kSilverMagic));

    // Identical recordings must serialize byte identical, so no host padding
    // can leak in.
    OreCommandBuffer buf2;
    recordRepresentative(buf2);
    std::vector<uint8_t> silver2;
    serializeSilver(buf2, silver2);
    CHECK(silver == silver2);

    CHECK(silverMatch(silver, silver2));
}

TEST_CASE("ore silver detects a diverging field", "[ore][cmd]")
{
    OreCommandBuffer expected;
    recordRepresentative(expected);
    std::vector<uint8_t> expectedSilver;
    serializeSilver(expected, expectedSilver);

    OreCommandBuffer actual;
    BeginRenderPassCmd begin{};
    begin.colorCount = 1;
    begin.colors[0] =
        {0, kInvalidHandle, LoadOp::clear, StoreOp::store, 0.f, 0.f, 0.f, 1.f};
    begin.depthStencil.view = kInvalidHandle;
    actual.append(CommandType::beginRenderPass, begin);
    actual.append(CommandType::draw, DrawCmd{99, 1, 0, 0});
    std::vector<uint8_t> actualSilver;
    serializeSilver(actual, actualSilver);

    CHECK_FALSE(silverMatch(expectedSilver, actualSilver));
}

TEST_CASE("ore silver tolerates sub-epsilon float drift", "[ore][cmd]")
{
    OreCommandBuffer a;
    a.append(CommandType::setBlendColor, SetBlendColorCmd{0.5f, 0.f, 0.f, 1.f});
    std::vector<uint8_t> silverA;
    serializeSilver(a, silverA);

    OreCommandBuffer b;
    b.append(CommandType::setBlendColor,
             SetBlendColorCmd{0.5f + kSilverEpsilon * 0.5f, 0.f, 0.f, 1.f});
    std::vector<uint8_t> silverB;
    serializeSilver(b, silverB);

    CHECK(silverMatch(silverA, silverB));
}
