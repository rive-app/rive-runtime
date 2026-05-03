/*
 * Copyright 2025 Rive
 */

// Combined D3D11 + D3D12 RenderPass implementation for Windows.
// Compiled when both ORE_BACKEND_D3D11 and ORE_BACKEND_D3D12 are active.
// Provides all RenderPass method definitions with runtime dispatch:
// passes initialized via the D3D11 context path (m_d3d11Context != nullptr)
// use D3D11; passes initialized via the D3D12 context path
// (m_d3dCmdList != nullptr) use D3D12.

#if defined(ORE_BACKEND_D3D11) && defined(ORE_BACKEND_D3D12)

// Pull in D3D12 static helpers (none currently, but keeps the pattern).
#include "ore_render_pass_d3d12.cpp"

// Pull in D3D11 static helpers (oreTopologyToD3D, oreIndexFormatToDXGI).
#include "../d3d11/ore_render_pass_d3d11.cpp"

#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/rive_types.hpp"
#include "ore_d3d12_bind_group_apply.hpp"

namespace rive::ore
{

// ============================================================================
// RenderPass — lifecycle
// ============================================================================

RenderPass::~RenderPass()
{
    if (!m_finished && (m_d3d11Context != nullptr || m_d3dCmdList != nullptr))
        finish();
}

RenderPass::RenderPass(RenderPass&& other) noexcept :
    // D3D11 members
    m_currentPipeline(std::move(other.m_currentPipeline)),
    m_d3d11Context(other.m_d3d11Context),
    m_d3d11Topology(other.m_d3d11Topology),
    m_d3d11IndexFormat(other.m_d3d11IndexFormat),
    m_d3d11IndexOffset(other.m_d3d11IndexOffset),
    m_d3d11StencilRef(other.m_d3d11StencilRef),
    m_d3d11ResolveCount(other.m_d3d11ResolveCount),
    // D3D12 members
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
{
    moveCrossBackendFieldsFrom(other);
    // D3D11 arrays
    m_d3d11BlendFactor[0] = other.m_d3d11BlendFactor[0];
    m_d3d11BlendFactor[1] = other.m_d3d11BlendFactor[1];
    m_d3d11BlendFactor[2] = other.m_d3d11BlendFactor[2];
    m_d3d11BlendFactor[3] = other.m_d3d11BlendFactor[3];
    for (uint32_t i = 0; i < m_d3d11ResolveCount; ++i)
        m_d3d11Resolves[i] = other.m_d3d11Resolves[i];
    // D3D12 arrays
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

    other.m_d3d11Context = nullptr;
    other.m_d3dCmdList = nullptr;
    other.m_d3dDevice = nullptr;
    other.m_d3dContext = nullptr;
    other.m_d3dDepthResource = nullptr;
    other.m_d3dDepthTexture = nullptr;
    other.m_d3dColorCount = 0;
    other.m_d3dResolveCount = 0;
}

RenderPass& RenderPass::operator=(RenderPass&& other) noexcept
{
    if (this != &other)
    {
        if (!m_finished &&
            (m_d3d11Context != nullptr || m_d3dCmdList != nullptr))
            finish();

        moveCrossBackendFieldsFrom(other);
        // D3D11
        m_d3d11Context = other.m_d3d11Context;
        m_currentPipeline = std::move(other.m_currentPipeline);
        m_d3d11Topology = other.m_d3d11Topology;
        m_d3d11IndexFormat = other.m_d3d11IndexFormat;
        m_d3d11IndexOffset = other.m_d3d11IndexOffset;
        m_d3d11StencilRef = other.m_d3d11StencilRef;
        m_d3d11BlendFactor[0] = other.m_d3d11BlendFactor[0];
        m_d3d11BlendFactor[1] = other.m_d3d11BlendFactor[1];
        m_d3d11BlendFactor[2] = other.m_d3d11BlendFactor[2];
        m_d3d11BlendFactor[3] = other.m_d3d11BlendFactor[3];
        m_d3d11ResolveCount = other.m_d3d11ResolveCount;
        for (uint32_t i = 0; i < m_d3d11ResolveCount; ++i)
            m_d3d11Resolves[i] = other.m_d3d11Resolves[i];
        // D3D12
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

        other.m_d3d11Context = nullptr;
        other.m_d3dCmdList = nullptr;
        other.m_d3dDevice = nullptr;
        other.m_d3dContext = nullptr;
        other.m_d3dDepthResource = nullptr;
        other.m_d3dDepthTexture = nullptr;
        other.m_d3dColorCount = 0;
        other.m_d3dResolveCount = 0;
    }
    return *this;
}

void RenderPass::validate() const
{
    assert(!m_finished && "RenderPass already finished");
    assert(m_d3d11Context != nullptr || m_d3dCmdList != nullptr);
}

// ============================================================================
// setPipeline
// ============================================================================

void RenderPass::setPipeline(Pipeline* pipeline)
{
    validate();
    if (!checkPipelineCompat(pipeline))
        return;
    if (m_d3d11Context)
    {
        m_currentPipeline = ref_rcp(pipeline);
        const auto& desc = pipeline->desc();

        m_d3d11Context->VSSetShader(pipeline->m_d3dVS, nullptr, 0);
        m_d3d11Context->PSSetShader(pipeline->m_d3dPS, nullptr, 0);
        m_d3d11Context->IASetInputLayout(pipeline->m_d3dInputLayout.Get());

        m_d3d11Topology = oreTopologyToD3D(desc.topology);
        m_d3d11Context->IASetPrimitiveTopology(m_d3d11Topology);

        m_d3d11Context->RSSetState(pipeline->m_d3dRasterizer.Get());
        m_d3d11Context->OMSetDepthStencilState(
            pipeline->m_d3dDepthStencil.Get(),
            m_d3d11StencilRef);
        m_d3d11Context->OMSetBlendState(pipeline->m_d3dBlend.Get(),
                                        m_d3d11BlendFactor,
                                        0xFFFFFFFF);
        return;
    }

    // D3D12 path.
    m_d3dCurrentPipeline = ref_rcp(pipeline);
    m_d3dCmdList->SetPipelineState(pipeline->m_d3dPSO.Get());
    m_d3dCmdList->IASetPrimitiveTopology(pipeline->m_d3dTopology);
    // Each pipeline owns its own root sig (RFC v5 §3.2.2); rebind on
    // change. See ore_render_pass_d3d12.cpp for the matching block —
    // both files must stay in sync on this logic.
    ID3D12RootSignature* rootSig = pipeline->m_d3dRootSigOwned.Get();
    if (rootSig != m_d3dCurrentRootSig)
    {
        m_d3dCmdList->SetGraphicsRootSignature(rootSig);
        m_d3dCurrentRootSig = rootSig;
    }
    m_d3dCmdList->OMSetStencilRef(m_d3dStencilRef);
    m_d3dCmdList->OMSetBlendFactor(m_d3dBlendFactor);
}

// ============================================================================
// setVertexBuffer
// ============================================================================

void RenderPass::setVertexBuffer(uint32_t slot, Buffer* buffer, uint32_t offset)
{
    validate();
    if (m_d3d11Context)
    {
        UINT stride = (m_currentPipeline &&
                       slot < m_currentPipeline->desc().vertexBufferCount)
                          ? m_currentPipeline->desc().vertexBuffers[slot].stride
                          : 0;
        ID3D11Buffer* buf = buffer->m_d3d11Buffer.Get();
        UINT off = offset;
        m_d3d11Context->IASetVertexBuffers(slot, 1, &buf, &stride, &off);
        return;
    }

    // D3D12 path.
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
}

// ============================================================================
// setIndexBuffer
// ============================================================================

void RenderPass::setIndexBuffer(Buffer* buffer,
                                IndexFormat format,
                                uint32_t offset)
{
    validate();
    if (m_d3d11Context)
    {
        m_d3d11IndexFormat = oreIndexFormatToDXGI(format);
        m_d3d11IndexOffset = offset;
        m_d3d11Context->IASetIndexBuffer(buffer->m_d3d11Buffer.Get(),
                                         m_d3d11IndexFormat,
                                         offset);
        return;
    }

    // D3D12 path.
    assert(buffer->m_d3dBuffer != nullptr);
    m_d3dIndexFormat = (format == IndexFormat::uint32) ? DXGI_FORMAT_R32_UINT
                                                       : DXGI_FORMAT_R16_UINT;
    m_d3dIndexOffset = offset;
    D3D12_INDEX_BUFFER_VIEW ibv = {};
    ibv.BufferLocation = buffer->m_d3dBuffer->GetGPUVirtualAddress() + offset;
    ibv.SizeInBytes = buffer->m_size - offset;
    ibv.Format = m_d3dIndexFormat;
    m_d3dCmdList->IASetIndexBuffer(&ibv);
}

// ============================================================================
// setBindGroup
// ============================================================================

void RenderPass::setBindGroup(uint32_t groupIndex,
                              BindGroup* bg,
                              const uint32_t* dynamicOffsets,
                              uint32_t dynamicOffsetCount)
{
    validate();
    assert(bg != nullptr);
    m_boundGroups[groupIndex] = ref_rcp(bg);

    if (m_d3d11Context)
    {
        // m_d3d11UBOs is sorted by WGSL `@binding` ascending at
        // makeBindGroup time, so `dynamicOffsets[i]` here pairs with
        // the i-th dynamic UBO in BindGroupLayout-entry order.
        uint32_t dynIdx = 0;
        ID3D11DeviceContext1* ctx1 =
            m_context ? m_context->m_d3d11Context1.Get() : nullptr;
        for (const auto& ubo : bg->m_d3d11UBOs)
        {
            ID3D11Buffer* buf = ubo.buffer;
            uint32_t offsetBytes = ubo.offset;
            if (ubo.hasDynamicOffset && dynIdx < dynamicOffsetCount)
            {
                offsetBytes += dynamicOffsets[dynIdx];
                ++dynIdx;
            }
            // D3D11.1 spec: `NumConstants` must be a multiple of 16
            // (256 bytes), minimum 16. NVIDIA enforces strictly —
            // passing 1 silently drops the offset and re-reads from
            // buffer start. See sibling fix in
            // `ore_render_pass_d3d11.cpp`.
            const bool useOffsetPath =
                (offsetBytes != 0 && ctx1 != nullptr && ubo.size != 0);
            const UINT firstConstant = useOffsetPath ? (offsetBytes / 16) : 0;
            const UINT numConstantsRaw = (ubo.size + 15) / 16;
            const UINT numConstants =
                useOffsetPath
                    ? (numConstantsRaw < 16 ? 16u
                                            : ((numConstantsRaw + 15u) & ~15u))
                    : 0;
            // Per-stage emit (RFC §3.4): VS / PS register namespaces are
            // independent; skip the stage whose slot is `kAbsent` so we
            // don't clobber another resource's register.
            if (ubo.vsSlot != BindingMap::kAbsent)
            {
                if (useOffsetPath)
                    ctx1->VSSetConstantBuffers1(ubo.vsSlot,
                                                1,
                                                &buf,
                                                &firstConstant,
                                                &numConstants);
                else
                    m_d3d11Context->VSSetConstantBuffers(ubo.vsSlot, 1, &buf);
            }
            if (ubo.psSlot != BindingMap::kAbsent)
            {
                if (useOffsetPath)
                    ctx1->PSSetConstantBuffers1(ubo.psSlot,
                                                1,
                                                &buf,
                                                &firstConstant,
                                                &numConstants);
                else
                    m_d3d11Context->PSSetConstantBuffers(ubo.psSlot, 1, &buf);
            }
        }
        for (const auto& tex : bg->m_d3d11Textures)
        {
            if (tex.vsSlot != BindingMap::kAbsent)
                m_d3d11Context->VSSetShaderResources(tex.vsSlot, 1, &tex.srv);
            if (tex.psSlot != BindingMap::kAbsent)
                m_d3d11Context->PSSetShaderResources(tex.psSlot, 1, &tex.srv);
        }
        for (const auto& samp : bg->m_d3d11Samplers)
        {
            if (samp.vsSlot != BindingMap::kAbsent)
                m_d3d11Context->VSSetSamplers(samp.vsSlot, 1, &samp.sampler);
            if (samp.psSlot != BindingMap::kAbsent)
                m_d3d11Context->PSSetSamplers(samp.psSlot, 1, &samp.sampler);
        }
        return;
    }

    // D3D12 path. Shared body lives in ore_d3d12_bind_group_apply.hpp so
    // the D3D12-only file (ore_render_pass_d3d12.cpp) and this combined
    // file call the same implementation.
    d3d12ApplyBindGroup(groupIndex, bg, dynamicOffsets, dynamicOffsetCount);
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
    validate();
    if (m_d3d11Context)
    {
        D3D11_VIEWPORT vp{};
        vp.TopLeftX = x;
        vp.TopLeftY = y;
        vp.Width = width;
        vp.Height = height;
        vp.MinDepth = minDepth;
        vp.MaxDepth = maxDepth;
        m_d3d11Context->RSSetViewports(1, &vp);
        D3D11_RECT scissor{};
        scissor.left = static_cast<LONG>(x);
        scissor.top = static_cast<LONG>(y);
        scissor.right = static_cast<LONG>(x + width);
        scissor.bottom = static_cast<LONG>(y + height);
        m_d3d11Context->RSSetScissorRects(1, &scissor);
        return;
    }

    // D3D12 path.
    D3D12_VIEWPORT vp = {x, y, width, height, minDepth, maxDepth};
    m_d3dCmdList->RSSetViewports(1, &vp);
    D3D12_RECT scissor = {(LONG)x,
                          (LONG)y,
                          (LONG)(x + width),
                          (LONG)(y + height)};
    m_d3dCmdList->RSSetScissorRects(1, &scissor);
}

void RenderPass::setScissorRect(uint32_t x,
                                uint32_t y,
                                uint32_t width,
                                uint32_t height)
{
    validate();
    if (m_d3d11Context)
    {
        D3D11_RECT rect{};
        rect.left = static_cast<LONG>(x);
        rect.top = static_cast<LONG>(y);
        rect.right = static_cast<LONG>(x + width);
        rect.bottom = static_cast<LONG>(y + height);
        m_d3d11Context->RSSetScissorRects(1, &rect);
        return;
    }

    // D3D12 path.
    D3D12_RECT rect = {(LONG)x, (LONG)y, (LONG)(x + width), (LONG)(y + height)};
    m_d3dCmdList->RSSetScissorRects(1, &rect);
}

// ============================================================================
// setStencilReference / setBlendColor
// ============================================================================

void RenderPass::setStencilReference(uint32_t ref)
{
    validate();
    if (m_d3d11Context)
    {
        m_d3d11StencilRef = ref;
        if (m_currentPipeline)
        {
            m_d3d11Context->OMSetDepthStencilState(
                m_currentPipeline->m_d3dDepthStencil.Get(),
                ref);
        }
        return;
    }

    // D3D12 path.
    m_d3dStencilRef = ref;
    m_d3dCmdList->OMSetStencilRef(ref);
}

void RenderPass::setBlendColor(float r, float g, float b, float a)
{
    validate();
    if (m_d3d11Context)
    {
        m_d3d11BlendFactor[0] = r;
        m_d3d11BlendFactor[1] = g;
        m_d3d11BlendFactor[2] = b;
        m_d3d11BlendFactor[3] = a;
        if (m_currentPipeline)
        {
            m_d3d11Context->OMSetBlendState(m_currentPipeline->m_d3dBlend.Get(),
                                            m_d3d11BlendFactor,
                                            0xFFFFFFFF);
        }
        return;
    }

    // D3D12 path.
    m_d3dBlendFactor[0] = r;
    m_d3dBlendFactor[1] = g;
    m_d3dBlendFactor[2] = b;
    m_d3dBlendFactor[3] = a;
    m_d3dCmdList->OMSetBlendFactor(m_d3dBlendFactor);
}

// ============================================================================
// draw / drawIndexed
// ============================================================================

void RenderPass::draw(uint32_t vertexCount,
                      uint32_t instanceCount,
                      uint32_t firstVertex,
                      uint32_t firstInstance)
{
    validate();
    if (m_d3d11Context)
    {
        if (instanceCount > 1 || firstInstance != 0)
        {
            m_d3d11Context->DrawInstanced(vertexCount,
                                          instanceCount,
                                          firstVertex,
                                          firstInstance);
        }
        else
        {
            m_d3d11Context->Draw(vertexCount, firstVertex);
        }
        return;
    }

    // D3D12 path.
    m_d3dCmdList->DrawInstanced(vertexCount,
                                instanceCount,
                                firstVertex,
                                firstInstance);
}

void RenderPass::drawIndexed(uint32_t indexCount,
                             uint32_t instanceCount,
                             uint32_t firstIndex,
                             int32_t baseVertex,
                             uint32_t firstInstance)
{
    validate();
    if (m_d3d11Context)
    {
        if (instanceCount > 1 || firstInstance != 0 || baseVertex != 0)
        {
            m_d3d11Context->DrawIndexedInstanced(indexCount,
                                                 instanceCount,
                                                 firstIndex,
                                                 baseVertex,
                                                 firstInstance);
        }
        else
        {
            m_d3d11Context->DrawIndexed(indexCount, firstIndex, 0);
        }
        return;
    }

    // D3D12 path.
    m_d3dCmdList->DrawIndexedInstanced(indexCount,
                                       instanceCount,
                                       firstIndex,
                                       baseVertex,
                                       firstInstance);
}

// ============================================================================
// finish
// ============================================================================

void RenderPass::finish()
{
    if (m_finished)
        return;
    m_finished = true;

    if (m_d3d11Context != nullptr)
    {
        // Unbind render targets so attached textures can be used as SRVs.
        m_d3d11Context->OMSetRenderTargets(0, nullptr, nullptr);

        // Resolve MSAA render targets to their 1x resolve targets.
        for (uint32_t i = 0; i < m_d3d11ResolveCount; ++i)
        {
            const auto& r = m_d3d11Resolves[i];
            m_d3d11Context->ResolveSubresource(r.resolve,
                                               r.resolveSubresource,
                                               r.msaa,
                                               r.msaaSubresource,
                                               r.format);
        }

        m_d3d11Context = nullptr;
        return;
    }

    if (m_d3dCmdList == nullptr)
        return;

    // MSAA resolve (D3D12 path). Mirrors ore_render_pass_d3d12.cpp.
    if (m_d3dResolveCount > 0)
    {
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

        for (uint32_t i = 0; i < m_d3dResolveCount; ++i)
        {
            const auto& r = m_d3dResolves[i];
            m_d3dCmdList->ResolveSubresource(r.resolve,
                                             r.resolveSubresource,
                                             r.msaa,
                                             r.msaaSubresource,
                                             r.format);
        }

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
            // Keep the resolve target's tracked state in sync.
            if (r.resolveTex != nullptr)
                r.resolveTex->m_d3dCurrentState = r.resolveFinalState;
        }
        if (postCount > 0)
            m_d3dCmdList->ResourceBarrier(postCount, post);
    }

    // Transition colour attachments back to their post-pass state. MSAA
    // srcs that were resolved are now in RESOLVE_SOURCE; others RENDER_TARGET.
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
}

} // namespace rive::ore

#endif // ORE_BACKEND_D3D11 && ORE_BACKEND_D3D12
