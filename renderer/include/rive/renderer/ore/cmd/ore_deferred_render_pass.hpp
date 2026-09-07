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

    // The base destructor can only reach its own finish, which drops the
    // drain.
    ~InlineDeferredRenderPass() override { finish(); }

    void finish() override
    {
        if (m_finished)
        {
            return;
        }
        RenderPassRecording::finish();
        replayCommandBuffer(*m_context, buffer);
    }
};

// The one entry point for a script facing pass. It always records, so a pass
// begun inside another finishes on its own instead of taking a live encoder.
inline std::unique_ptr<RenderPass> beginRecordedRenderPass(
    Context& ctx,
    const RenderPassDesc& desc)
{
    if (ctx.isRecording())
    {
        return ctx.beginRenderPass(desc);
    }
    if (ctx.deferredRecording() && ctx.usesDeferredFrameReplay())
    {
        return std::make_unique<RenderPassRecording>(&ctx,
                                                     &ctx.pendingFrame(),
                                                     desc);
    }
    return std::make_unique<InlineDeferredRenderPass>(&ctx, desc);
}

} // namespace rive::ore::cmd
