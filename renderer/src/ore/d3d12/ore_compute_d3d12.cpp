/*
 * Copyright 2026 Rive
 */

#include "ore_compute_d3d12.hpp"
#include "ore_bind_group_d3d12.hpp"
#include "rive/renderer/ore/ore_context_d3d12.hpp"

#include <climits>

namespace rive::ore
{
#if defined(ORE_BACKEND_D3D12)

ComputePassD3D12::ComputePassD3D12(ContextD3D12* context)
    : ComputePass(context)
    , m_d3d12(context)
{}

ComputePassD3D12::~ComputePassD3D12() { finish(); }

void ComputePassD3D12::applyPipeline(ComputePipeline*) {}

bool ComputePassD3D12::setGroupTables(const ComputePipelineD3D12& pipeline)
{
  for (uint32_t g = 0; g < kMaxBindGroups; ++g)
  {
    const D3D12GroupRootParams& params = pipeline.m_d3dGroupParams[g];
    const bool views = params.cbvSrvUavRootParamIdx >= 0;
    const bool samplers = params.samplerRootParamIdx >= 0;
    if (!views && !samplers)
      continue;
    auto* group = lite_rtti_cast<BindGroupD3D12*>(m_groups[g].get());
    if (group == nullptr)
    {
      // A table left unset would be read by the shader all the same.
      m_d3d12->setLastError(
          "dispatchWorkgroups: @group(%u): the pipeline binds this group "
          "but none is set",
          g);
      return false;
    }
    const UINT viewStart = !views ? 0
                                  : m_d3d12->d3d12AllocGpuSlots(
                                        m_d3d12->m_d3dGpuSrvHeap,
                                        uint32_t(params.cbvCount) +
                                            params.srvCount + params.uavCount);
    const UINT samplerStart = !samplers ? 0
                                        : m_d3d12->d3d12AllocGpuSlots(
                                              m_d3d12->m_d3dGpuSamplerHeap,
                                              params.samplerCount);
    if (viewStart == UINT_MAX || samplerStart == UINT_MAX)
      return false;
    m_d3d12->d3d12SetGroupTables(
        params,
        group,
        m_groupOffsets[g],
        m_groupOffsetCounts[g],
        viewStart,
        samplerStart,
        true);
  }
  return true;
}

void ComputePassD3D12::dispatch(uint32_t x, uint32_t y, uint32_t z)
{
  ID3D12GraphicsCommandList* list = m_d3d12->m_d3dCmdList;
  auto* pipeline = lite_rtti_cast<ComputePipelineD3D12*>(m_pipeline.get());
  if (list == nullptr || pipeline == nullptr)
    return;
  list->SetComputeRootSignature(pipeline->m_d3dRootSig.Get());
  list->SetPipelineState(pipeline->m_d3dPSO.Get());
  // Again if a heap filled meanwhile, since that dropped the tables set
  // before it.
  uint32_t generation;
  do
  {
    generation = m_d3d12->m_d3dGpuHeapGeneration;
    if (!setGroupTables(*pipeline))
      return;
  } while (generation != m_d3d12->m_d3dGpuHeapGeneration);
  const uint32_t counts[3] = {x, y, z};
  list->SetComputeRoot32BitConstants(
      static_cast<UINT>(pipeline->m_d3dWorkgroupCountParam),
      3,
      counts,
      0);
  list->Dispatch(x, y, z);
}

void ComputePassD3D12::end()
{
  if (m_d3d12->m_d3dCmdList != nullptr)
    m_d3d12->d3d12RestStorage();
  m_d3d12->m_d3dPassOpen = false;
  m_d3d12->m_d3dComputeOpen = false;
}

#endif // ORE_BACKEND_D3D12
} // namespace rive::ore
