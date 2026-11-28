/*
 * Copyright 2026 Rive
 *
 * Shared D3D12 body for `RenderPass::setBindGroup`. Takes the slots for one
 * merged CBV/SRV/UAV descriptor table per group + one sampler table per
 * group under the per-pipeline root sig (RFC v5 §3.2.2 / "Commit 7b"), which
 * `ContextD3D12::d3d12SetGroupTables` then writes as it does for a dispatch.
 * See `ore_d3d12_root_sig.hpp` for the root-sig layout.
 *
 * Defined inline here because two translation units need to call it:
 * the D3D12-only build compiles `ore_render_pass_d3d12.cpp`
 * standalone, and the combined Windows build compiles
 * `ore_render_pass_d3d11_d3d12.cpp` (which runtime-dispatches to
 * D3D11 or D3D12 based on which context opened the pass). Before
 * this header both files carried a copy of the same logic.
 */

#pragma once

#include "rive/renderer/ore/ore_context_d3d12.hpp"
#include "ore_render_pass_d3d12.hpp"
#include "ore_bind_group_d3d12.hpp"
#include "ore_buffer_d3d12.hpp"
#include "ore_pipeline_d3d12.hpp"

#include <d3d12.h>
#include <cassert>
#include <climits>
#include <cstdint>
#include "ore_texture_d3d12.hpp"

namespace rive::ore
{

inline void RenderPassD3D12::d3d12ApplyBindGroup(uint32_t groupIndex)
{
  assert(groupIndex < 4); // RFC v4 §9.1 / §14.2.

  auto bg = lite_rtti_cast<BindGroupD3D12*>(m_boundGroups[groupIndex].get());
  const uint32_t* dynamicOffsets = m_groupOffsets[groupIndex];
  const uint32_t dynamicOffsetCount = m_groupOffsetCounts[groupIndex];
  PipelineD3D12* pipe = m_d3dCurrentPipeline.get();
  assert(
      pipe != nullptr && pipe->m_d3dRootSigOwned &&
      "d3d12ApplyBindGroup: pipeline missing per-pipeline root sig — "
      "every Ore D3D12 pipeline must ship with a populated binding map");

  const auto& gp = pipe->m_d3dGroupParams[groupIndex];

  // Both ranges before any table, since taking either can swap a full heap,
  // which drops the tables already set.
  UINT srvStart = 0;
  UINT samplerStart = 0;
  if (gp.cbvSrvUavRootParamIdx >= 0)
  {
    srvStart = m_d3dContext->d3d12AllocGpuSlots(
        m_d3dContext->m_d3dGpuSrvHeap,
        uint32_t(gp.cbvCount) + uint32_t(gp.srvCount) + uint32_t(gp.uavCount));
  }
  if (gp.samplerRootParamIdx >= 0)
  {
    samplerStart = m_d3dContext->d3d12AllocGpuSlots(
        m_d3dContext->m_d3dGpuSamplerHeap,
        gp.samplerCount);
  }
  if (m_d3dHeapGeneration != m_d3dContext->m_d3dGpuHeapGeneration)
  {
    m_d3dHeapGeneration = m_d3dContext->m_d3dGpuHeapGeneration;
    unapplyBoundGroups();
  }
  if (srvStart == UINT_MAX || samplerStart == UINT_MAX)
  {
    // The allocator set lastError. Draws skip while this group stays
    // unapplied.
    m_unappliedGroups |= groupBit(groupIndex);
    return;
  }
  m_unappliedGroups &= static_cast<uint8_t>(~groupBit(groupIndex));
  m_d3dContext->d3d12SetGroupTables(
      gp,
      bg,
      dynamicOffsets,
      dynamicOffsetCount,
      srvStart,
      samplerStart,
      false);
}

} // namespace rive::ore
