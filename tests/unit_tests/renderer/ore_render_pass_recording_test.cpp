/*
 * Copyright 2026 Rive
 */

// A RenderPassRecording must emit exactly the stream a hand built
// OreCommandBuffer would, compared by silver. Null resources capture as
// kInvalidHandle so no GPU is required.

#include "rive/renderer/ore/cmd/ore_render_pass_recording.hpp"
#include "rive/renderer/ore/cmd/ore_command_silver.hpp"

#include <catch.hpp>
#include <vector>

using namespace rive::ore;
using namespace rive::ore::cmd;

TEST_CASE("RenderPassRecording emits the expected command stream", "[ore][cmd]")
{
    RenderPassDesc desc;
    desc.colorCount = 1;
    desc.colorAttachments[0].view = nullptr;
    desc.colorAttachments[0].resolveTarget = nullptr;
    desc.colorAttachments[0].loadOp = LoadOp::clear;
    desc.colorAttachments[0].storeOp = StoreOp::store;
    desc.colorAttachments[0].clearColor = {0.25f, 0.5f, 0.75f, 1.0f};
    desc.depthStencil.view = nullptr;

    OreCommandBuffer recorded;
    {
        // A null context is safe, validation only dereferences it to report
        // errors.
        RenderPassRecording pass(nullptr, &recorded, desc);
        pass.setPipeline(nullptr);
        pass.setViewport(0.f, 0.f, 128.f, 64.f, 0.f, 1.f);
        pass.setScissorRect(0, 0, 128, 64);
        pass.draw(6, 1, 0, 0);
        pass.finish();
    }

    // Hand built reference stream.
    OreCommandBuffer expected;
    BeginRenderPassCmd begin{};
    begin.colorCount = 1;
    begin.colors[0] = {kInvalidHandle,
                       kInvalidHandle,
                       LoadOp::clear,
                       StoreOp::store,
                       0.25f,
                       0.5f,
                       0.75f,
                       1.0f};
    begin.depthStencil.view = kInvalidHandle;
    begin.depthStencil.depthLoadOp = LoadOp::clear;
    begin.depthStencil.depthStoreOp = StoreOp::store;
    begin.depthStencil.depthClearValue = 1.0f;
    begin.depthStencil.stencilLoadOp = LoadOp::clear;
    begin.depthStencil.stencilStoreOp = StoreOp::discard;
    begin.depthStencil.stencilClearValue = 0;
    expected.append(CommandType::beginRenderPass, begin);
    // A null pipeline records an invalid handle.
    expected.append(CommandType::setPipeline, SetPipelineCmd{kInvalidHandle});
    expected.append(CommandType::setViewport,
                    SetViewportCmd{0.f, 0.f, 128.f, 64.f, 0.f, 1.f});
    expected.append(CommandType::setScissorRect,
                    SetScissorRectCmd{0, 0, 128, 64});
    expected.append(CommandType::draw, DrawCmd{6, 1, 0, 0});
    expected.appendOpcode(CommandType::finish);

    std::vector<uint8_t> recordedSilver, expectedSilver;
    serializeSilver(recorded, recordedSilver);
    serializeSilver(expected, expectedSilver);
    CHECK(silverMatch(expectedSilver, recordedSilver));
}

TEST_CASE("RenderPassRecording finish is idempotent", "[ore][cmd]")
{
    RenderPassDesc desc;
    desc.colorCount = 0;
    desc.depthStencil.view = nullptr;

    OreCommandBuffer recorded;
    RenderPassRecording pass(nullptr, &recorded, desc);
    pass.draw(3, 1, 0, 0);
    pass.finish();
    CHECK(pass.isFinished());
    pass.finish();

    OreCommandReader r(recorded.commandBytes(), recorded.blobBytes());
    CommandType t;
    int finishes = 0;
    int total = 0;
    while (r.next(t))
    {
        ++total;
        if (t == CommandType::finish)
        {
            ++finishes;
            continue;
        }
        // The reader requires consuming each payload.
        switch (t)
        {
            case CommandType::beginRenderPass:
                r.read<BeginRenderPassCmd>();
                break;
            case CommandType::draw:
                r.read<DrawCmd>();
                break;
            default:
                FAIL("unexpected command in stream");
        }
    }
    CHECK(finishes == 1);
    CHECK(total == 3);
}
