/*
 * Copyright 2026 Rive
 */

#include "ore_compute_wgpu.hpp"
#include "ore_bind_group_wgpu.hpp"
#include "rive/renderer/ore/ore_context_wgpu.hpp"

namespace rive::ore
{

ComputePassWGPU::ComputePassWGPU(
    ContextWGPU* context,
    wgpu::ComputePassEncoder encoder)
    : ComputePass(context)
    , m_wgpuContext(context)
    , m_wgpuPassEncoder(std::move(encoder))
{}

ComputePassWGPU::~ComputePassWGPU() { finish(); }

void ComputePassWGPU::applyPipeline(ComputePipeline* pipeline)
{
  m_wgpuPassEncoder.SetPipeline(
      static_cast<ComputePipelineWGPU*>(pipeline)->m_wgpuPipeline);
}

void ComputePassWGPU::dispatch(uint32_t x, uint32_t y, uint32_t z)
{
  for (uint32_t g = 0; g < kMaxBindGroups; ++g)
  {
    // A group left at an index this pipeline binds nothing at stays out.
    auto* group = lite_rtti_cast<BindGroupWGPU*>(m_groups[g].get());
    if (group == nullptr || !hasEntries(m_pipeline->m_layouts[g]))
      continue;
    const wgpu::BindGroup& native = group->resolveBindGroup();
    if (native == nullptr)
    {
      m_wgpuContext->setLastError(
          "ore: WGPU bind group creation failed for group %u",
          g);
      return;
    }
    group->markUBOsBound();
    m_wgpuPassEncoder
        .SetBindGroup(g, native, m_groupOffsetCounts[g], m_groupOffsets[g]);
  }
  m_wgpuPassEncoder.DispatchWorkgroups(x, y, z);
}

void ComputePassWGPU::end()
{
  m_wgpuPassEncoder.End();
  m_wgpuPassEncoder = nullptr;
  m_wgpuContext->m_wgpuPassOpen = false;
}

} // namespace rive::ore
