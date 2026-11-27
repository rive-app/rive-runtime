/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/cmd/ore_compute_pass_recording.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_render_pass.hpp"

// The compute siblings of InlineDeferredRenderPass and
// beginRecordedRenderPass.
namespace rive::ore::cmd
{

class InlineDeferredComputePass : private OwnedOreCommandBuffer,
                                  public ComputePassRecording
{
public:
  InlineDeferredComputePass(Context* context, const ComputePassDesc& desc)
      : ComputePassRecording(context, &buffer, desc)
  {}

  // The base destructor would only reach its own end, which drops the drain.
  ~InlineDeferredComputePass() override { finish(); }

protected:
  void end() override
  {
    ComputePassRecording::end();
    replayCommandBuffer(*m_context, buffer);
  }
};

// The one entry point for a script facing compute pass. It records, so the
// pass keeps its place among the recorded render passes.
inline std::unique_ptr<ComputePass> beginRecordedComputePass(
    Context& ctx,
    const ComputePassDesc& desc,
    std::string* outError = nullptr)
{
  // A device without compute refuses in its own beginComputePass.
  if (ctx.isRecording() || !ctx.features().computeShaders)
  {
    return ctx.beginComputePass(desc, outError);
  }
  if (ctx.deferredRecording() && ctx.usesDeferredFrameReplay())
  {
    return std::make_unique<ComputePassRecording>(
        &ctx,
        &ctx.pendingFrame(),
        desc);
  }
  return std::make_unique<InlineDeferredComputePass>(&ctx, desc);
}

} // namespace rive::ore::cmd
