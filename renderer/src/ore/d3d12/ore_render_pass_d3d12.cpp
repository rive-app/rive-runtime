/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/rive_types.hpp"
#include "ore_d3d12_bind_group_apply.hpp"

#include <d3d12.h>
#include <cassert>
#include <cstring>

namespace rive::ore
{

// --- Public method definitions (D3D12-only builds) ---
// When both D3D11 and D3D12 are compiled, the combined
// ore_context_d3d11_d3d12.cpp file provides these methods with dispatch.
#if defined(ORE_BACKEND_D3D12) && !defined(ORE_BACKEND_D3D11)

// ============================================================================
// RenderPass — lifecycle
// ============================================================================

RenderPass::~RenderPass()
{
    if (!m_finished && m_d3dCmdList != nullptr)
        finish();
}

RenderPass::RenderPass(RenderPass&& other) noexcept
#if defined(ORE_BACKEND_D3D12)
    :
    m_d3dCmdList(other.m_d3dCmdList),
    m_d3dDevice(other.m_d3dDevice),
    m_d3dContext(other.m_d3dContext),
    m_d3dCurrentPipeline(std::move(other.m_d3dCurrentPipeline)),
    m_d3dIndexFormat(other.m_d3dIndexFormat),
    m_d3dIndexOffset(other.m_d3dIndexOffset),
    m_d3dStencilRef(other.m_d3dStencilRef),
    m_d3dColorCount(other.m_d3dColorCount),
    m_d3dDepthResource(other.m_d3dDepthResource),
    m_d3dDepthFinalState(other.m_d3dDepthFinalState)
#endif
{
    moveCrossBackendFieldsFrom(other);
#if defined(ORE_BACKEND_D3D12)
    for (int i = 0; i < 8; ++i)
    {
        m_d3dPendingUBOs[i] = other.m_d3dPendingUBOs[i];
        m_d3dPendingSrvHandles[i] = other.m_d3dPendingSrvHandles[i];
        m_d3dPendingSamplerHandles[i] = other.m_d3dPendingSamplerHandles[i];
    }
    for (int i = 0; i < 4; ++i)
    {
        m_d3dBlendFactor[i] = other.m_d3dBlendFactor[i];
        m_d3dColorResources[i] = other.m_d3dColorResources[i];
        m_d3dColorTextures[i] = other.m_d3dColorTextures[i];
        m_d3dColorFinalStates[i] = other.m_d3dColorFinalStates[i];
        m_d3dResolves[i] = other.m_d3dResolves[i];
    }
    m_d3dDepthTexture = other.m_d3dDepthTexture;
    m_d3dResolveCount = other.m_d3dResolveCount;
    other.m_d3dCmdList = nullptr;
    other.m_d3dDevice = nullptr;
    other.m_d3dContext = nullptr;
    other.m_d3dDepthResource = nullptr;
    other.m_d3dDepthTexture = nullptr;
    other.m_d3dColorCount = 0;
    other.m_d3dResolveCount = 0;
#endif
}

RenderPass& RenderPass::operator=(RenderPass&& other) noexcept
{
    if (this != &other)
    {
        if (!m_finished && m_d3dCmdList != nullptr)
            finish();
        moveCrossBackendFieldsFrom(other);
#if defined(ORE_BACKEND_D3D12)
        m_d3dCmdList = other.m_d3dCmdList;
        m_d3dDevice = other.m_d3dDevice;
        m_d3dContext = other.m_d3dContext;
        m_d3dCurrentPipeline = std::move(other.m_d3dCurrentPipeline);
        m_d3dIndexFormat = other.m_d3dIndexFormat;
        m_d3dIndexOffset = other.m_d3dIndexOffset;
        m_d3dStencilRef = other.m_d3dStencilRef;
        m_d3dColorCount = other.m_d3dColorCount;
        m_d3dDepthResource = other.m_d3dDepthResource;
        m_d3dDepthFinalState = other.m_d3dDepthFinalState;
        for (int i = 0; i < 8; ++i)
        {
            m_d3dPendingUBOs[i] = other.m_d3dPendingUBOs[i];
            m_d3dPendingSrvHandles[i] = other.m_d3dPendingSrvHandles[i];
            m_d3dPendingSamplerHandles[i] = other.m_d3dPendingSamplerHandles[i];
        }
        for (int i = 0; i < 4; ++i)
        {
            m_d3dBlendFactor[i] = other.m_d3dBlendFactor[i];
            m_d3dColorResources[i] = other.m_d3dColorResources[i];
            m_d3dColorTextures[i] = other.m_d3dColorTextures[i];
            m_d3dColorFinalStates[i] = other.m_d3dColorFinalStates[i];
            m_d3dResolves[i] = other.m_d3dResolves[i];
        }
        m_d3dDepthTexture = other.m_d3dDepthTexture;
        m_d3dResolveCount = other.m_d3dResolveCount;
        other.m_d3dCmdList = nullptr;
        other.m_d3dDevice = nullptr;
        other.m_d3dContext = nullptr;
        other.m_d3dDepthResource = nullptr;
        other.m_d3dDepthTexture = nullptr;
        other.m_d3dColorCount = 0;
        other.m_d3dResolveCount = 0;
#endif
    }
    return *this;
}

void RenderPass::validate() const
{
    assert(!m_finished && "RenderPass already finished");
#if defined(ORE_BACKEND_D3D12)
    assert(m_d3dCmdList != nullptr);
#endif
}

// ============================================================================
// setPipeline
// ============================================================================

void RenderPass::setPipeline(Pipeline* pipeline)
{
#if defined(ORE_BACKEND_D3D12)
    validate();
    if (!checkPipelineCompat(pipeline))
        return;
    m_d3dCurrentPipeline = ref_rcp(pipeline);

    m_d3dCmdList->SetPipelineState(pipeline->m_d3dPSO.Get());
    m_d3dCmdList->IASetPrimitiveTopology(pipeline->m_d3dTopology);
    // Each pipeline owns its own root sig (RFC v5 §3.2.2). Rebind on
    // change; D3D12 resets all root-parameter state when the sig
    // changes, so callers must rebind bind groups after any pipeline
    // switch that flips sig.
    ID3D12RootSignature* rootSig = pipeline->m_d3dRootSigOwned.Get();
    if (rootSig != m_d3dCurrentRootSig)
    {
        m_d3dCmdList->SetGraphicsRootSignature(rootSig);
        m_d3dCurrentRootSig = rootSig;
    }
    // Stencil / blend colour — re-apply with new pipeline state.
    m_d3dCmdList->OMSetStencilRef(m_d3dStencilRef);
    m_d3dCmdList->OMSetBlendFactor(m_d3dBlendFactor);
#else
    (void)pipeline;
#endif
}

// ============================================================================
// setVertexBuffer
// ============================================================================

void RenderPass::setVertexBuffer(uint32_t slot, Buffer* buffer, uint32_t offset)
{
#if defined(ORE_BACKEND_D3D12)
    validate();
    assert(buffer->m_d3dBuffer != nullptr);

    UINT stride = (m_d3dCurrentPipeline &&
                   slot < m_d3dCurrentPipeline->desc().vertexBufferCount)
                      ? m_d3dCurrentPipeline->m_d3dVertexStrides[slot]
                      : 0;

    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = buffer->m_d3dBuffer->GetGPUVirtualAddress() + offset;
    vbv.SizeInBytes = buffer->m_size - offset;
    vbv.StrideInBytes = stride;
    m_d3dCmdList->IASetVertexBuffers(slot, 1, &vbv);
#else
    (void)slot;
    (void)buffer;
    (void)offset;
#endif
}

// ============================================================================
// setIndexBuffer
// ============================================================================

void RenderPass::setIndexBuffer(Buffer* buffer,
                                IndexFormat format,
                                uint32_t offset)
{
#if defined(ORE_BACKEND_D3D12)
    validate();
    assert(buffer->m_d3dBuffer != nullptr);

    m_d3dIndexFormat = (format == IndexFormat::uint32) ? DXGI_FORMAT_R32_UINT
                                                       : DXGI_FORMAT_R16_UINT;
    m_d3dIndexOffset = offset;

    D3D12_INDEX_BUFFER_VIEW ibv = {};
    ibv.BufferLocation = buffer->m_d3dBuffer->GetGPUVirtualAddress() + offset;
    ibv.SizeInBytes = buffer->m_size - offset;
    ibv.Format = m_d3dIndexFormat;
    m_d3dCmdList->IASetIndexBuffer(&ibv);
#else
    (void)buffer;
    (void)format;
    (void)offset;
#endif
}

void RenderPass::setBindGroup(uint32_t groupIndex,
                              BindGroup* bg,
                              const uint32_t* dynamicOffsets,
                              uint32_t dynamicOffsetCount)
{
#if defined(ORE_BACKEND_D3D12)
    validate();
    assert(bg != nullptr);
    // Hold a strong reference so the BindGroup stays alive until finish().
    m_boundGroups[groupIndex] = ref_rcp(bg);
    d3d12ApplyBindGroup(groupIndex, bg, dynamicOffsets, dynamicOffsetCount);
#else
    (void)groupIndex;
    (void)bg;
    (void)dynamicOffsets;
    (void)dynamicOffsetCount;
#endif
}

// ============================================================================
// setViewport / setScissorRect
// ============================================================================

void RenderPass::setViewport(float x,
                             float y,
                             float width,
                             float height,
                             float minDepth,
                             float maxDepth)
{
#if defined(ORE_BACKEND_D3D12)
    validate();
    D3D12_VIEWPORT vp = {x, y, width, height, minDepth, maxDepth};
    m_d3dCmdList->RSSetViewports(1, &vp);

    // Reset scissor to full viewport.
    D3D12_RECT scissor = {(LONG)x,
                          (LONG)y,
                          (LONG)(x + width),
                          (LONG)(y + height)};
    m_d3dCmdList->RSSetScissorRects(1, &scissor);
#else
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)minDepth;
    (void)maxDepth;
#endif
}

void RenderPass::setScissorRect(uint32_t x,
                                uint32_t y,
                                uint32_t width,
                                uint32_t height)
{
#if defined(ORE_BACKEND_D3D12)
    validate();
    D3D12_RECT rect = {(LONG)x, (LONG)y, (LONG)(x + width), (LONG)(y + height)};
    m_d3dCmdList->RSSetScissorRects(1, &rect);
#else
    (void)x;
    (void)y;
    (void)width;
    (void)height;
#endif
}

// ============================================================================
// setStencilReference / setBlendColor
// ============================================================================

void RenderPass::setStencilReference(uint32_t ref)
{
#if defined(ORE_BACKEND_D3D12)
    validate();
    m_d3dStencilRef = ref;
    m_d3dCmdList->OMSetStencilRef(ref);
#else
    (void)ref;
#endif
}

void RenderPass::setBlendColor(float r, float g, float b, float a)
{
#if defined(ORE_BACKEND_D3D12)
    validate();
    m_d3dBlendFactor[0] = r;
    m_d3dBlendFactor[1] = g;
    m_d3dBlendFactor[2] = b;
    m_d3dBlendFactor[3] = a;
    m_d3dCmdList->OMSetBlendFactor(m_d3dBlendFactor);
#else
    (void)r;
    (void)g;
    (void)b;
    (void)a;
#endif
}

// ============================================================================
// draw / drawIndexed
// ============================================================================

void RenderPass::draw(uint32_t vertexCount,
                      uint32_t instanceCount,
                      uint32_t firstVertex,
                      uint32_t firstInstance)
{
#if defined(ORE_BACKEND_D3D12)
    validate();
    m_d3dCmdList->DrawInstanced(vertexCount,
                                instanceCount,
                                firstVertex,
                                firstInstance);
#else
    (void)vertexCount;
    (void)instanceCount;
    (void)firstVertex;
    (void)firstInstance;
#endif
}

void RenderPass::drawIndexed(uint32_t indexCount,
                             uint32_t instanceCount,
                             uint32_t firstIndex,
                             int32_t baseVertex,
                             uint32_t firstInstance)
{
#if defined(ORE_BACKEND_D3D12)
    validate();
    m_d3dCmdList->DrawIndexedInstanced(indexCount,
                                       instanceCount,
                                       firstIndex,
                                       baseVertex,
                                       firstInstance);
#else
    (void)indexCount;
    (void)instanceCount;
    (void)firstIndex;
    (void)baseVertex;
    (void)firstInstance;
#endif
}

// ============================================================================
// finish
// ============================================================================

void RenderPass::finish()
{
    if (m_finished)
        return;
    m_finished = true;

#if defined(ORE_BACKEND_D3D12)
    if (m_d3dCmdList == nullptr)
        return;

    // MSAA resolve, if any. Pattern per resolve pair:
    //   msaa: RENDER_TARGET → RESOLVE_SOURCE
    //   dst:  prior-state   → RESOLVE_DEST
    //   ResolveSubresource
    //   msaa: RESOLVE_SOURCE → final colour state (handled by loop below)
    //   dst:  RESOLVE_DEST   → dst's final state (handled here, since it
    //                          isn't a colour attachment of this pass)
    // A single barrier batch is issued for each phase so the driver can
    // schedule efficiently.
    if (m_d3dResolveCount > 0)
    {
        // Phase 1: transition MSAA srcs to RESOLVE_SOURCE and resolve dsts
        // to RESOLVE_DEST.
        D3D12_RESOURCE_BARRIER pre[8] = {};
        uint32_t preCount = 0;
        for (uint32_t i = 0; i < m_d3dResolveCount; ++i)
        {
            const auto& r = m_d3dResolves[i];
            pre[preCount].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            pre[preCount].Transition.pResource = r.msaa;
            pre[preCount].Transition.StateBefore =
                D3D12_RESOURCE_STATE_RENDER_TARGET;
            pre[preCount].Transition.StateAfter =
                D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
            pre[preCount].Transition.Subresource = r.msaaSubresource;
            ++preCount;

            pre[preCount].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            pre[preCount].Transition.pResource = r.resolve;
            pre[preCount].Transition.StateBefore = r.resolvePriorState;
            pre[preCount].Transition.StateAfter =
                D3D12_RESOURCE_STATE_RESOLVE_DEST;
            pre[preCount].Transition.Subresource = r.resolveSubresource;
            ++preCount;
        }
        m_d3dCmdList->ResourceBarrier(preCount, pre);

        // Phase 2: execute resolves.
        for (uint32_t i = 0; i < m_d3dResolveCount; ++i)
        {
            const auto& r = m_d3dResolves[i];
            m_d3dCmdList->ResolveSubresource(r.resolve,
                                             r.resolveSubresource,
                                             r.msaa,
                                             r.msaaSubresource,
                                             r.format);
        }

        // Phase 3: transition resolve dsts back. MSAA srcs still need a
        // RESOLVE_SOURCE → final-state transition, which we fold into the
        // colour-final-state loop below by adjusting StateBefore.
        D3D12_RESOURCE_BARRIER post[4] = {};
        uint32_t postCount = 0;
        for (uint32_t i = 0; i < m_d3dResolveCount; ++i)
        {
            const auto& r = m_d3dResolves[i];
            post[postCount].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            post[postCount].Transition.pResource = r.resolve;
            post[postCount].Transition.StateBefore =
                D3D12_RESOURCE_STATE_RESOLVE_DEST;
            post[postCount].Transition.StateAfter = r.resolveFinalState;
            post[postCount].Transition.Subresource = r.resolveSubresource;
            ++postCount;
            // Keep the resolve target's tracked state in sync with the
            // actual resource state so its next use (begin of a future pass,
            // or an upload, etc.) sees the right StateBefore.
            if (r.resolveTex != nullptr)
                r.resolveTex->m_d3dCurrentState = r.resolveFinalState;
        }
        if (postCount > 0)
            m_d3dCmdList->ResourceBarrier(postCount, post);
    }

    // Transition colour attachments back to their post-pass state.
    // MSAA srcs that were resolved are in RESOLVE_SOURCE; others are in
    // RENDER_TARGET. Keep Texture::m_d3dCurrentState synced so the next
    // use of the texture sees the right StateBefore.
    for (uint32_t i = 0; i < m_d3dColorCount; ++i)
    {
        if (!m_d3dColorResources[i])
            continue;

        D3D12_RESOURCE_STATES stateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        for (uint32_t r = 0; r < m_d3dResolveCount; ++r)
        {
            if (m_d3dResolves[r].msaa == m_d3dColorResources[i])
            {
                stateBefore = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
                break;
            }
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = m_d3dColorResources[i];
        barrier.Transition.StateBefore = stateBefore;
        barrier.Transition.StateAfter = m_d3dColorFinalStates[i];
        barrier.Transition.Subresource =
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_d3dCmdList->ResourceBarrier(1, &barrier);

        if (m_d3dColorTextures[i] != nullptr)
            m_d3dColorTextures[i]->m_d3dCurrentState = m_d3dColorFinalStates[i];
    }

    // Transition depth/stencil if present.
    if (m_d3dDepthResource)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = m_d3dDepthResource;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrier.Transition.StateAfter = m_d3dDepthFinalState;
        barrier.Transition.Subresource =
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_d3dCmdList->ResourceBarrier(1, &barrier);

        if (m_d3dDepthTexture != nullptr)
            m_d3dDepthTexture->m_d3dCurrentState = m_d3dDepthFinalState;
    }

    m_d3dCmdList = nullptr;
    m_d3dDevice = nullptr;
    m_d3dContext = nullptr;
#endif
}

#endif // D3D12-only

} // namespace rive::ore
