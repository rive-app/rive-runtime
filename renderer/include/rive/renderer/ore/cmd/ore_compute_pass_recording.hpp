/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/cmd/ore_render_pass_recording.hpp"
#include "rive/renderer/ore/ore_compute_pass.hpp"

// Deferred mode ore::ComputePass, recording as RenderPassRecording does.
namespace rive::ore::cmd
{

class ComputePassRecording : public ComputePass
{
public:
  ComputePassRecording(
      Context* context,
      OreCommandBuffer* cmd,
      const ComputePassDesc& desc)
      : ComputePass(context)
      , m_cmd(cmd)
  {
    BeginComputePassCmd begin{m_cmd->appendStringRef(desc.label)};
    if (m_context != nullptr)
    {
      m_context->beginOpenPass(this, *m_cmd);
    }
    m_cmd->append(CommandType::beginComputePass, begin);
  }

  // A begin with no finish would corrupt the stream at replay.
  ~ComputePassRecording() override { finish(); }

  void setBindGroup(
      uint32_t groupIndex,
      BindGroup* bg,
      const uint32_t* dynamicOffsets = nullptr,
      uint32_t dynamicOffsetCount = 0) override
  {
    if (isFinished())
    {
      return;
    }
    // Hold a strong reference so GC cannot free the group before replay.
    if (groupIndex < kMaxBindGroups)
    {
      m_groups[groupIndex] = ref_rcp(bg);
    }
    settlePass(m_context, this, *m_cmd);
    noteBoundStorage(m_context, this, bg);
    recordSetBindGroup(
        *m_cmd,
        groupIndex,
        bg,
        dynamicOffsets,
        dynamicOffsetCount);
  }

protected:
  void applyPipeline(ComputePipeline* pipeline) override
  {
    settlePass(m_context, this, *m_cmd);
    m_cmd->append(
        CommandType::setComputePipeline,
        SetPipelineCmd{
            recordedHandle<DeferredComputePipeline>(*m_cmd, pipeline)});
  }

  void dispatch(uint32_t x, uint32_t y, uint32_t z) override
  {
    settlePass(m_context, this, *m_cmd);
    m_cmd->append(
        CommandType::dispatchWorkgroups,
        DispatchWorkgroupsCmd{x, y, z});
  }

  void end() override
  {
    if (m_context != nullptr)
    {
      m_context->finishNestedPasses(this);
    }
    m_cmd->appendOpcode(CommandType::finish);
    if (m_context != nullptr)
    {
      m_context->finishOpenPass(this);
    }
  }

private:
  OreCommandBuffer* m_cmd;
};

} // namespace rive::ore::cmd
