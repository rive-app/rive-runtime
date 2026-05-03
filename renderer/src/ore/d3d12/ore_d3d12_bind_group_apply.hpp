/*
 * Copyright 2026 Rive
 *
 * Shared D3D12 body for `RenderPass::setBindGroup`. Builds one merged
 * CBV/SRV/UAV descriptor table per group + one sampler table per group
 * using the per-pipeline root sig (RFC v5 §3.2.2 / "Commit 7b"). See
 * `ore_d3d12_root_sig.hpp` for the root-sig layout.
 *
 * Defined inline here because two translation units need to call it:
 * the D3D12-only build compiles `ore_render_pass_d3d12.cpp`
 * standalone, and the combined Windows build compiles
 * `ore_render_pass_d3d11_d3d12.cpp` (which runtime-dispatches to
 * D3D11 or D3D12 based on which context opened the pass). Before
 * this header both files carried a copy of the same logic.
 */

#pragma once

#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"

#include <d3d12.h>
#include <cassert>
#include <climits>
#include <cstdint>

namespace rive::ore
{

inline void RenderPass::d3d12ApplyBindGroup(uint32_t groupIndex,
                                            BindGroup* bg,
                                            const uint32_t* dynamicOffsets,
                                            uint32_t dynamicOffsetCount)
{
    assert(groupIndex < 4); // RFC v4 §9.1 / §14.2.

    Pipeline* pipe = m_d3dCurrentPipeline.get();
    assert(pipe != nullptr && pipe->m_d3dRootSigOwned &&
           "d3d12ApplyBindGroup: pipeline missing per-pipeline root sig — "
           "every Ore D3D12 pipeline must ship with a populated binding map");

    const auto& gp = pipe->m_d3dGroupParams[groupIndex];
    uint32_t dynIdx = 0;

    // Merged CBV/SRV/UAV table.
    if (gp.cbvSrvUavRootParamIdx >= 0)
    {
        uint32_t total = uint32_t(gp.cbvCount) + uint32_t(gp.srvCount) +
                         uint32_t(gp.uavCount);
        UINT heapStart = m_d3dContext->d3d12AllocGpuSrvSlots(total);
        if (heapStart == UINT_MAX)
        {
            // Heap exhausted — `setLastError` was already populated by
            // the allocator. Skip this table; subsequent draw will read
            // whatever (stale) descriptors are still bound from the
            // last successful call. Better than the prior assert/hang.
            return;
        }
        uint32_t offset = 0;

        // CBVs: create D3D12_CONSTANT_BUFFER_VIEW_DESC descriptors in
        // the shader-visible heap on the fly, one per slot covered by
        // this group's CBV range. Null CBVs are left as default-init
        // descriptor slots (CreateConstantBufferView(nullptr, ...)
        // writes the D3D12-defined null CBV per the spec).
        for (uint8_t i = 0; i < gp.cbvCount; ++i)
        {
            uint8_t globalSlot = gp.cbvBaseGlobalSlot + i;
            D3D12_CPU_DESCRIPTOR_HANDLE dst =
                m_d3dContext->d3d12GpuSrvCpuHandle(heapStart + offset++);
            if (bg->m_d3dUBOSlotMask & (1u << globalSlot))
            {
                D3D12_GPU_VIRTUAL_ADDRESS gpuVA =
                    bg->m_d3dUBOAddresses[globalSlot];
                if ((bg->m_d3dUBODynamicOffsetMask & (1u << globalSlot)) &&
                    dynIdx < dynamicOffsetCount)
                {
                    gpuVA += dynamicOffsets[dynIdx];
                    ++dynIdx;
                }
                // CBV sizes must be 256-byte aligned per D3D12 spec.
                uint32_t size = bg->m_d3dUBOSizes[globalSlot];
                size = (size + 255u) & ~255u;
                D3D12_CONSTANT_BUFFER_VIEW_DESC cbv = {};
                cbv.BufferLocation = gpuVA;
                cbv.SizeInBytes = size;
                m_d3dDevice->CreateConstantBufferView(&cbv, dst);
            }
            else
            {
                m_d3dDevice->CreateConstantBufferView(nullptr, dst);
            }
        }

        // SRVs: copy each group-local SRV slot's CPU handle into the
        // heap. Missing slots fall back to the context's null SRV
        // descriptor so the table stays well-defined.
        //
        // Resource-state transitions: D3D12 requires textures bound as
        // SRVs to be in a shader-resource state for the stages that
        // sample them. Ore doesn't know which stages a given binding
        // group will be sampled from at apply time, so transition into
        // the *union* state — `PIXEL_SHADER_RESOURCE` for fragment
        // samples plus `NON_PIXEL_SHADER_RESOURCE` for vertex/etc.
        // samples — and require the texture to *already* cover both
        // before skipping the barrier.
        //
        // Two cases this catches:
        //   1. Depth-stencil texture born in `DEPTH_WRITE` (because
        //      `renderTarget=true`) and never attached as a DSV before
        //      being sampled. `ore_binding_shadow_sampler_d32` GM.
        //   2. Color attachment finished into just
        //      `PIXEL_SHADER_RESOURCE` by an earlier render pass, then
        //      sampled from a *vertex* shader. `ore_binding_vs_texture`
        //      GM. Without `NON_PIXEL_SHADER_RESOURCE`, GBV fires
        //      `id=1358 Incompatible texture barrier layout: Shader
        //      Stage: VERTEX, Layout:
        //      D3D12_BARRIER_LAYOUT_LEGACY_PIXEL_SHADER_RESOURCE`.
        //
        // Without these barriers GPU-Based Validation eventually TDRs
        // the device with `DXGI_ERROR_DEVICE_HUNG`.
        constexpr D3D12_RESOURCE_STATES kSrvStates =
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        for (uint8_t i = 0; i < gp.srvCount; ++i)
        {
            uint8_t globalSlot = gp.srvBaseGlobalSlot + i;
            if (!(bg->m_d3dSrvSlotMask & (1u << globalSlot)))
                continue;
            Texture* tex = bg->m_d3dSrvTextures[globalSlot];
            if (tex == nullptr)
                continue;
            // Require *full* coverage of both shader-stage bits, not
            // just any overlap. A texture in just
            // `PIXEL_SHADER_RESOURCE` (typical post-finish state for
            // color attachments) needs the `NON_PIXEL_SHADER_RESOURCE`
            // bit added before a vertex-stage sample.
            if ((tex->m_d3dCurrentState & kSrvStates) == kSrvStates)
                continue;
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = tex->m_d3dTexture.Get();
            barrier.Transition.StateBefore = tex->m_d3dCurrentState;
            barrier.Transition.StateAfter = kSrvStates;
            barrier.Transition.Subresource =
                D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            m_d3dCmdList->ResourceBarrier(1, &barrier);
            tex->m_d3dCurrentState = kSrvStates;
        }
        for (uint8_t i = 0; i < gp.srvCount; ++i)
        {
            uint8_t globalSlot = gp.srvBaseGlobalSlot + i;
            D3D12_CPU_DESCRIPTOR_HANDLE dst =
                m_d3dContext->d3d12GpuSrvCpuHandle(heapStart + offset++);
            D3D12_CPU_DESCRIPTOR_HANDLE src =
                (bg->m_d3dSrvSlotMask & (1u << globalSlot))
                    ? bg->m_d3dSrvHandles[globalSlot]
                    : m_d3dContext->m_d3dNullSrv;
            m_d3dDevice->CopyDescriptorsSimple(
                1,
                dst,
                src,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        // UAVs: Ore doesn't ship storage buffers/textures on day 1,
        // but the helper reserves slots for forward-compat. Null-fill
        // for now; the loop no-ops at gp.uavCount == 0.
        for (uint8_t i = 0; i < gp.uavCount; ++i)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE dst =
                m_d3dContext->d3d12GpuSrvCpuHandle(heapStart + offset++);
            m_d3dDevice->CopyDescriptorsSimple(
                1,
                dst,
                m_d3dContext->m_d3dNullSrv,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle =
            m_d3dContext->d3d12GpuSrvGpuHandle(heapStart);
        m_d3dCmdList->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(gp.cbvSrvUavRootParamIdx),
            gpuHandle);
    }

    // Sampler table (separate heap type).
    if (gp.samplerRootParamIdx >= 0)
    {
        UINT heapStart =
            m_d3dContext->d3d12AllocGpuSamplerSlots(gp.samplerCount);
        if (heapStart == UINT_MAX)
            return;
        for (uint8_t i = 0; i < gp.samplerCount; ++i)
        {
            uint8_t globalSlot = gp.samplerBaseGlobalSlot + i;
            D3D12_CPU_DESCRIPTOR_HANDLE dst =
                m_d3dContext->d3d12GpuSamplerCpuHandle(heapStart + i);
            D3D12_CPU_DESCRIPTOR_HANDLE src =
                (bg->m_d3dSamplerSlotMask & (1u << globalSlot))
                    ? bg->m_d3dSamplerHandles[globalSlot]
                    : m_d3dContext->m_d3dNullSampler;
            m_d3dDevice->CopyDescriptorsSimple(
                1,
                dst,
                src,
                D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        }
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle =
            m_d3dContext->d3d12GpuSamplerGpuHandle(heapStart);
        m_d3dCmdList->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(gp.samplerRootParamIdx),
            gpuHandle);
    }
}

} // namespace rive::ore
