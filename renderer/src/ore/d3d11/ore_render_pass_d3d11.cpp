/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_context.hpp" // for RenderPass inline bodies
#include "rive/rive_types.hpp"

#include <d3d11.h>

namespace rive::ore
{

// ============================================================================
// D3D11 static helpers — used by both the standalone D3D11-only RenderPass
// methods below and by the combined D3D11+D3D12 dispatch file
// (ore_render_pass_d3d11_d3d12.cpp) which #includes this file.
// ============================================================================

#if defined(ORE_BACKEND_D3D11)

[[maybe_unused]] static D3D11_PRIMITIVE_TOPOLOGY oreTopologyToD3D(
    PrimitiveTopology topo)
{
    switch (topo)
    {
        case PrimitiveTopology::pointList:
            return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        case PrimitiveTopology::lineList:
            return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveTopology::lineStrip:
            return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case PrimitiveTopology::triangleList:
            return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveTopology::triangleStrip:
            return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    }
    RIVE_UNREACHABLE();
}

[[maybe_unused]] static DXGI_FORMAT oreIndexFormatToDXGI(IndexFormat format)
{
    switch (format)
    {
        case IndexFormat::uint16:
            return DXGI_FORMAT_R16_UINT;
        case IndexFormat::uint32:
            return DXGI_FORMAT_R32_UINT;
        case IndexFormat::none:
            break;
    }
    RIVE_UNREACHABLE();
}

#endif // ORE_BACKEND_D3D11

// ============================================================================
// RenderPass — D3D11-only method definitions
// ============================================================================

#if defined(ORE_BACKEND_D3D11) && !defined(ORE_BACKEND_D3D12)

RenderPass::~RenderPass()
{
    if (!m_finished && m_d3d11Context != nullptr)
    {
        finish();
    }
}

RenderPass::RenderPass(RenderPass&& other) noexcept :
    m_currentPipeline(std::move(other.m_currentPipeline)),
    m_d3d11Context(other.m_d3d11Context),
    m_d3d11Topology(other.m_d3d11Topology),
    m_d3d11IndexFormat(other.m_d3d11IndexFormat),
    m_d3d11IndexOffset(other.m_d3d11IndexOffset),
    m_d3d11StencilRef(other.m_d3d11StencilRef),
    m_d3d11ResolveCount(other.m_d3d11ResolveCount)
{
    moveCrossBackendFieldsFrom(other);
    m_d3d11BlendFactor[0] = other.m_d3d11BlendFactor[0];
    m_d3d11BlendFactor[1] = other.m_d3d11BlendFactor[1];
    m_d3d11BlendFactor[2] = other.m_d3d11BlendFactor[2];
    m_d3d11BlendFactor[3] = other.m_d3d11BlendFactor[3];
    for (uint32_t i = 0; i < m_d3d11ResolveCount; ++i)
        m_d3d11Resolves[i] = other.m_d3d11Resolves[i];
    other.m_d3d11Context = nullptr;
}

RenderPass& RenderPass::operator=(RenderPass&& other) noexcept
{
    if (this != &other)
    {
        if (!m_finished && m_d3d11Context != nullptr)
        {
            finish();
        }
        moveCrossBackendFieldsFrom(other);
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
        other.m_d3d11Context = nullptr;
    }
    return *this;
}

void RenderPass::validate() const
{
    assert(!m_finished && "RenderPass already finished");
    assert(m_d3d11Context != nullptr);
}

void RenderPass::setPipeline(Pipeline* pipeline)
{
    validate();
    if (!checkPipelineCompat(pipeline))
        return;
    m_currentPipeline = ref_rcp(pipeline);

    const auto& desc = pipeline->desc();

    m_d3d11Context->VSSetShader(pipeline->m_d3dVS, nullptr, 0);
    m_d3d11Context->PSSetShader(pipeline->m_d3dPS, nullptr, 0);
    m_d3d11Context->IASetInputLayout(pipeline->m_d3dInputLayout.Get());

    m_d3d11Topology = oreTopologyToD3D(desc.topology);
    m_d3d11Context->IASetPrimitiveTopology(m_d3d11Topology);

    m_d3d11Context->RSSetState(pipeline->m_d3dRasterizer.Get());
    m_d3d11Context->OMSetDepthStencilState(pipeline->m_d3dDepthStencil.Get(),
                                           m_d3d11StencilRef);
    m_d3d11Context->OMSetBlendState(pipeline->m_d3dBlend.Get(),
                                    m_d3d11BlendFactor,
                                    0xFFFFFFFF);
}

void RenderPass::setVertexBuffer(uint32_t slot, Buffer* buffer, uint32_t offset)
{
    validate();
    UINT stride = (m_currentPipeline &&
                   slot < m_currentPipeline->desc().vertexBufferCount)
                      ? m_currentPipeline->desc().vertexBuffers[slot].stride
                      : 0;
    ID3D11Buffer* buf = buffer->m_d3d11Buffer.Get();
    UINT off = offset;
    m_d3d11Context->IASetVertexBuffers(slot, 1, &buf, &stride, &off);
}

void RenderPass::setIndexBuffer(Buffer* buffer,
                                IndexFormat format,
                                uint32_t offset)
{
    validate();
    m_d3d11IndexFormat = oreIndexFormatToDXGI(format);
    m_d3d11IndexOffset = offset;
    m_d3d11Context->IASetIndexBuffer(buffer->m_d3d11Buffer.Get(),
                                     m_d3d11IndexFormat,
                                     offset);
}

void RenderPass::setBindGroup(uint32_t groupIndex,
                              BindGroup* bg,
                              const uint32_t* dynamicOffsets,
                              uint32_t dynamicOffsetCount)
{
    validate();
    assert(bg != nullptr);

    // Hold a strong reference so the BindGroup stays alive until finish().
    m_boundGroups[groupIndex] = ref_rcp(bg);

    // m_d3d11UBOs is sorted by WGSL `@binding` ascending at makeBindGroup
    // time, so `dynamicOffsets[i]` here pairs with the i-th dynamic UBO
    // in BindGroupLayout-entry order — matching WebGPU semantics
    // independently of the caller's `desc.ubos[]` order.
    uint32_t dynIdx = 0;

    // Bind UBOs per-stage. If an offset (static or dynamic) is non-zero,
    // route through VSSetConstantBuffers1 / PSSetConstantBuffers1 whose
    // firstConstant / numConstants take units of 16 bytes (one shader
    // constant). D3D11 additionally requires firstConstant to be a
    // multiple of 16 constants (256 bytes) — WebGPU's
    // minUniformBufferOffsetAlignment default matches this, so a
    // spec-compliant caller's offsets are already aligned.
    //
    // Per-stage emit (RFC §3.4): VS and PS register namespaces are
    // independent on D3D11; skip the stage whose slot is `kAbsent` so we
    // don't clobber another resource's register.
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
        // ubo.size was resolved to a concrete byte count at
        // makeBindGroup time (see d3d11MakeBindGroup). `0` can only
        // occur for an impossibly 0-sized buffer — fall back to
        // whole-buffer bind in that case.
        //
        // D3D11.1 spec: `NumConstants` must be a multiple of 16
        // constants (= 256 bytes). NVIDIA's driver enforces this
        // strictly — passing `1` (= 16 bytes, our default for a
        // single vec4 UBO) silently drops the offset and falls back
        // to binding from the buffer's start, producing wrong
        // colors on `ore_binding_dynamic_ubo` (CI regen flagged a
        // red-instead-of-yellow render on `nvidia_quadrop4000/d3datomic`).
        // Round up to the spec-required multiple of 16, with a
        // minimum of 16 (the smallest legal value). The shader
        // still only reads `ubo.size` bytes; binding more is
        // harmless for the shader and required for the API.
        const bool useOffsetPath =
            (offsetBytes != 0 && ctx1 != nullptr && ubo.size != 0);
        const UINT firstConstant = useOffsetPath ? (offsetBytes / 16) : 0;
        const UINT numConstantsRaw = (ubo.size + 15) / 16;
        const UINT numConstants =
            useOffsetPath
                ? (numConstantsRaw < 16 ? 16u
                                        : ((numConstantsRaw + 15u) & ~15u))
                : 0;
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

    // SRVs and samplers, per-stage. Skipping kAbsent stages (rather than
    // binding to both unconditionally) means a VS-only or PS-only
    // resource can share a register number with a different resource in
    // the other stage — matches RFC §3.4's per-stage register file.
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
}

void RenderPass::setViewport(float x,
                             float y,
                             float width,
                             float height,
                             float minDepth,
                             float maxDepth)
{
    validate();
    D3D11_VIEWPORT vp{};
    vp.TopLeftX = x;
    vp.TopLeftY = y;
    vp.Width = width;
    vp.Height = height;
    vp.MinDepth = minDepth;
    vp.MaxDepth = maxDepth;
    m_d3d11Context->RSSetViewports(1, &vp);
    // The rasterizer state always has ScissorEnable=TRUE. Reset the scissor
    // rect to cover the full viewport so draws are not silently clipped when
    // the caller has not issued an explicit setScissorRect. D3D11 initialises
    // scissor rects to {0,0,0,0} which discards every pixel.
    D3D11_RECT scissor{};
    scissor.left = static_cast<LONG>(x);
    scissor.top = static_cast<LONG>(y);
    scissor.right = static_cast<LONG>(x + width);
    scissor.bottom = static_cast<LONG>(y + height);
    m_d3d11Context->RSSetScissorRects(1, &scissor);
}

void RenderPass::setScissorRect(uint32_t x,
                                uint32_t y,
                                uint32_t width,
                                uint32_t height)
{
    validate();
    D3D11_RECT rect{};
    rect.left = static_cast<LONG>(x);
    rect.top = static_cast<LONG>(y);
    rect.right = static_cast<LONG>(x + width);
    rect.bottom = static_cast<LONG>(y + height);
    m_d3d11Context->RSSetScissorRects(1, &rect);
}

void RenderPass::setStencilReference(uint32_t ref)
{
    validate();
    m_d3d11StencilRef = ref;
    if (m_currentPipeline)
    {
        m_d3d11Context->OMSetDepthStencilState(
            m_currentPipeline->m_d3dDepthStencil.Get(),
            ref);
    }
}

void RenderPass::setBlendColor(float r, float g, float b, float a)
{
    validate();
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
}

void RenderPass::draw(uint32_t vertexCount,
                      uint32_t instanceCount,
                      uint32_t firstVertex,
                      uint32_t firstInstance)
{
    validate();
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
}

void RenderPass::drawIndexed(uint32_t indexCount,
                             uint32_t instanceCount,
                             uint32_t firstIndex,
                             int32_t baseVertex,
                             uint32_t firstInstance)
{
    validate();
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
}

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
    }
}

#endif // ORE_BACKEND_D3D11 && !ORE_BACKEND_D3D12

} // namespace rive::ore
