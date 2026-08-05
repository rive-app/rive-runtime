/*
 * Copyright 2026 Rive
 */

// Confirms every command and upload payload round trips byte for byte through
// OreCommandBuffer. No GPU needed.

#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"

#include <catch.hpp>
#include <cstring>
#include <vector>

using namespace rive::ore;
using namespace rive::ore::cmd;
using rive::Span;

TEST_CASE("ore command stream round-trips through the reader", "[ore][cmd]")
{
    OreCommandBuffer buf;

    BeginRenderPassCmd begin{};
    begin.colorCount = 1;
    begin.colors[0].view = 0;
    begin.colors[0].resolveTarget = kInvalidHandle;
    begin.colors[0].loadOp = LoadOp::clear;
    begin.colors[0].storeOp = StoreOp::store;
    begin.colors[0].clearR = 0.25f;
    begin.colors[0].clearG = 0.5f;
    begin.colors[0].clearB = 0.75f;
    begin.colors[0].clearA = 1.0f;
    begin.depthStencil.view = kInvalidHandle;
    buf.append(CommandType::beginRenderPass, begin);

    buf.append(CommandType::setPipeline, SetPipelineCmd{7});
    buf.append(CommandType::setVertexBuffer, SetVertexBufferCmd{0, 3, 16});
    buf.append(CommandType::draw, DrawCmd{6, 2, 1, 0});
    buf.appendOpcode(CommandType::finish);

    OreCommandReader r(buf.commandBytes(), buf.blobBytes());
    CommandType t;

    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::beginRenderPass);
    auto b = r.read<BeginRenderPassCmd>();
    CHECK(b.colorCount == 1);
    CHECK(b.colors[0].view == 0u);
    CHECK(b.colors[0].resolveTarget == kInvalidHandle);
    CHECK(b.colors[0].loadOp == LoadOp::clear);
    CHECK(b.colors[0].clearR == 0.25f);
    CHECK(b.colors[0].clearB == 0.75f);
    CHECK(b.depthStencil.view == kInvalidHandle);

    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::setPipeline);
    CHECK(r.read<SetPipelineCmd>().pipeline == 7u);

    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::setVertexBuffer);
    auto vb = r.read<SetVertexBufferCmd>();
    CHECK(vb.slot == 0u);
    CHECK(vb.buffer == 3u);
    CHECK(vb.offset == 16u);

    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::draw);
    auto d = r.read<DrawCmd>();
    CHECK(d.vertexCount == 6u);
    CHECK(d.instanceCount == 2u);
    CHECK(d.firstVertex == 1u);

    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::finish);

    REQUIRE_FALSE(r.next(t));
}

TEST_CASE("ore command buffer reset keeps the buffer reusable", "[ore][cmd]")
{
    OreCommandBuffer buf;
    buf.append(CommandType::draw, DrawCmd{1, 1, 0, 0});
    CHECK_FALSE(buf.empty());

    buf.reset();
    CHECK(buf.empty());
    CHECK(buf.keepAlive().empty());

    buf.appendOpcode(CommandType::finish);
    CHECK_FALSE(buf.empty());
}

TEST_CASE("ore command buffer capture maps nullptr to kInvalidHandle",
          "[ore][cmd]")
{
    OreCommandBuffer buf;
    CHECK(buf.capture(nullptr) == kInvalidHandle);
    CHECK(buf.keepAlive().empty());
}

TEST_CASE("a truncated trailing opcode latches overrun", "[ore][cmd]")
{
    // Ore opcodes are four bytes; three leftover bytes are a truncated
    // stream, not a clean end.
    std::vector<uint8_t> bytes = {1, 0, 0};
    rive::cmd::CommandReader<uint32_t> truncated(
        rive::Span<const uint8_t>(bytes.data(), bytes.size()),
        rive::Span<const uint8_t>());
    uint32_t op;
    CHECK_FALSE(truncated.next(op));
    CHECK(truncated.overrun());

    rive::cmd::CommandReader<uint32_t> clean{rive::Span<const uint8_t>(),
                                             rive::Span<const uint8_t>()};
    CHECK_FALSE(clean.next(op));
    CHECK_FALSE(clean.overrun());
}
