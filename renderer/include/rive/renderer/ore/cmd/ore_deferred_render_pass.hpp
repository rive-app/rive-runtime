/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"
#include "rive/renderer/ore/cmd/ore_render_pass_recording.hpp"
#include "rive/renderer/ore/cmd/ore_replay.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include <memory>

// Single threaded record then replay inline. InlineDeferredRenderPass records
// every call into an owned OreCommandBuffer and finish() drains it back
// through the live immediate path, so output is byte identical to immediate
// mode and callers need no changes.
namespace rive::ore::cmd
{

// Orders the owned buffer before the recording base that writes into it.
struct OwnedOreCommandBuffer
{
    OreCommandBuffer buffer;
};

class InlineDeferredRenderPass : private OwnedOreCommandBuffer,
                                 public RenderPassRecording
{
public:
    InlineDeferredRenderPass(Context* context, const RenderPassDesc& desc) :
        RenderPassRecording(context, &buffer, desc)
    {}

    void finish() override
    {
        if (m_finished)
        {
            return;
        }
        // The base latches m_finished before we replay: replay reenters
        // beginRenderPass, whose finishActiveRenderPass would otherwise call
        // this again.
        RenderPassRecording::finish();
        replayCommandBuffer(*m_context, buffer);
    }
};

// Single decision point between recording and the live immediate pass.
inline std::unique_ptr<RenderPass> beginRenderPassRecordingOrImmediate(
    Context& ctx,
    const RenderPassDesc& desc,
    std::string* outError = nullptr)
{
    if (ctx.deferredRecording())
    {
        if (ctx.usesDeferredFrameReplay())
        {
            // The backend replays the pending frame once at endFrame.
            return std::make_unique<RenderPassRecording>(&ctx,
                                                         &ctx.pendingFrame(),
                                                         desc);
        }
        // No frame boundary drain on this backend, replay the pass inline.
        return std::make_unique<InlineDeferredRenderPass>(&ctx, desc);
    }
    return ctx.beginRenderPass(desc, outError);
}

} // namespace rive::ore::cmd
