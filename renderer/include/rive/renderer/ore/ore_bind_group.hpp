/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"

#include <vector>

#if defined(ORE_BACKEND_METAL)
#import <Metal/Metal.h>
#endif
#if defined(ORE_BACKEND_D3D11)
#include <d3d11.h>
#include <wrl/client.h>
#endif
#if defined(ORE_BACKEND_D3D12)
#include <d3d12.h>
#include <wrl/client.h>
#endif
#if defined(ORE_BACKEND_WGPU)
#include <webgpu/webgpu_cpp.h>
#endif
#if defined(ORE_BACKEND_VK)
#include <vulkan/vulkan.h>
#endif

namespace rive::ore
{

class Context;

// Pre-baked resource bindings that can be reused across many draw calls.
// Holds strong rcp<> references to all bound resources (Buffer, TextureView,
// Sampler), ensuring they remain alive for the BindGroup's lifetime.
//
// Created via Context::makeBindGroup(). Bound via RenderPass::setBindGroup().
class BindGroup : public RefCnt<BindGroup>
{
public:
    uint32_t dynamicOffsetCount() const { return m_dynamicOffsetCount; }
    uint32_t groupIndex() const
    {
        return m_layoutRef ? m_layoutRef->groupIndex() : 0;
    }
    BindGroupLayout* layout() const { return m_layoutRef.get(); }
    Context* context() const { return m_context; }

private:
    friend class Context;
    friend class RenderPass;

    BindGroup() = default;

    uint32_t m_dynamicOffsetCount = 0;

    // The layout this BindGroup conforms to. Holds the per-backend native
    // layout handle alive for the BindGroup's lifetime — Vulkan's
    // VkDescriptorSetLayout in particular must outlive every VkDescriptorSet
    // allocated from it. Replaces the prior `m_vkPipelineRef` (which kept
    // the layout alive transitively via the pipeline).
    rcp<BindGroupLayout> m_layoutRef;

    // Lifecycle: hold rcp<> refs to all bound resources so they stay alive
    // even if the caller drops their references before the BindGroup is
    // destroyed.
    std::vector<rcp<Buffer>> m_retainedBuffers;
    std::vector<rcp<TextureView>> m_retainedViews;
    std::vector<rcp<Sampler>> m_retainedSamplers;

    // Context back-pointer set in makeBindGroup(). Used by the Lua GC
    // to call context->deferBindGroupDestroy() instead of dropping the
    // last rcp<> directly, keeping the object alive until endFrame().
    Context* m_context = nullptr;

    // Vulkan: VkDescriptorSetLayout lifetime is governed by `m_layoutRef`
    // above (the BindGroupLayout owns the DSL handle). The descriptor set
    // is allocated from Context's persistent pool at makeBindGroup time.
#if defined(ORE_BACKEND_VK)
    VkDescriptorSet m_vkDescriptorSet = VK_NULL_HANDLE;
#endif
#if defined(ORE_BACKEND_WGPU)
    wgpu::BindGroup m_wgpuBindGroup;
    // wgpu handles are internally ref-counted; no extra Pipeline ref needed.
#endif
#if defined(ORE_BACKEND_D3D12)
    // Descriptors pre-copied to CPU heap at makeBindGroup() time.
    // Copied to GPU heap at setBindGroup() time.
    //
    // Arrays are indexed by SLOT (0-7, matching the shader's `register(t/s/b)`
    // index), not by insertion order. Unbound slots have their bit cleared in
    // the corresponding *SlotMask; setBindGroup() fills them with null
    // descriptors so the root-signature-declared 8-slot tables are fully
    // populated. Storing by slot is essential: the root sig's SRV and sampler
    // descriptor tables assume GPU-heap slot == shader register, so packing by
    // insertion order (the prior layout) silently read from wrong slots when
    // the script used a non-dense binding layout.
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dSrvHandles[8] = {};
    // Weak refs to the underlying textures bound at each SRV slot. Used by
    // `d3d12ApplyBindGroup` to insert a state-transition barrier when the
    // texture isn't already in a shader-resource state (e.g. a
    // depth-stencil texture born in `DEPTH_WRITE` and never attached to a
    // render pass — the GBV `id=1358 Incompatible texture barrier
    // layout` case the `ore_binding_shadow_sampler_d32` GM hits).
    // `m_retainedViews` keeps the views alive; the textures are owned by
    // those views, so the raw pointers here are safe for the BindGroup's
    // lifetime.
    Texture* m_d3dSrvTextures[8] = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dSamplerHandles[8] = {};
    D3D12_GPU_VIRTUAL_ADDRESS m_d3dUBOAddresses[8] = {};
    // UBO byte sizes per slot. Used by `setBindGroup` to create CBV
    // descriptors on-the-fly into the shader-visible heap (D3D12 CBVs
    // live inside the merged CBV/SRV/UAV descriptor table).
    uint32_t m_d3dUBOSizes[8] = {};
    uint8_t m_d3dSrvSlotMask = 0;     // bit i = slot i has a bound SRV
    uint8_t m_d3dSamplerSlotMask = 0; // bit i = slot i has a bound sampler
    uint8_t m_d3dUBOSlotMask = 0;     // bit i = slot i has a bound UBO
    // bit i = slot i's UBO has hasDynamicOffset=true. setBindGroup() consumes
    // one entry from dynamicOffsets[] per set bit, in ascending slot order.
    uint8_t m_d3dUBODynamicOffsetMask = 0;
#endif
#if defined(ORE_BACKEND_METAL)
    // Metal: flat resource arrays applied at setBindGroup time.
    //
    // Per-stage Metal slots are stored independently per resource: Metal
    // has separate VS / FS argument tables, and the allocator (RFC §3.2.1)
    // is free to assign different per-stage slots — VS-only UBO A and
    // FS-only UBO B can share Metal slot 0 in their own stages. Each
    // `*Slot` is `BindingMap::kAbsent` when the binding isn't visible to
    // that stage; setBindGroup then skips the per-stage emit instead of
    // forwarding `0` (which would clobber another resource's slot).
    //
    // `binding` carries the WGSL `@binding` value, used as the sort key
    // for dynamic-offset ordering: WebGPU spec requires `dynamicOffsets[i]`
    // to be consumed in BindGroupLayout entry order, which (per RFC §3.6)
    // is ascending `@binding` within a kind. Sorting at construction time
    // makes the BindGroup robust to callers who pass `desc.ubos[]` in any
    // order.
    struct MTLBufferBinding
    {
        id<MTLBuffer> buffer = nil;
        uint32_t offset = 0;
        uint32_t binding = 0; // WGSL @binding — sort key for dynamicOffsets[].
        uint16_t vsSlot = 0xFFFF; // BindingMap::kAbsent.
        uint16_t fsSlot = 0xFFFF;
        // Populated from `BindGroupLayout::hasDynamicOffset(binding)` at
        // makeBindGroup time. setBindGroup consumes one `dynamicOffsets[]`
        // entry per `hasDynamicOffset=true` UBO in `binding`-ascending order.
        bool hasDynamicOffset = false;
    };
    struct MTLTextureBinding
    {
        id<MTLTexture> texture = nil;
        uint16_t vsSlot = 0xFFFF;
        uint16_t fsSlot = 0xFFFF;
    };
    struct MTLSamplerBinding
    {
        id<MTLSamplerState> sampler = nil;
        uint16_t vsSlot = 0xFFFF;
        uint16_t fsSlot = 0xFFFF;
    };
    std::vector<MTLBufferBinding> m_mtlBuffers;
    std::vector<MTLTextureBinding> m_mtlTextures;
    std::vector<MTLSamplerBinding> m_mtlSamplers;
#endif
#if defined(ORE_BACKEND_GL)
    // GL: single slot per binding (GL has a flat global namespace across
    // stages, no per-stage register split). `binding` carries the WGSL
    // `@binding` value, used as the sort key for dynamic-offset ordering
    // (consumed in BindGroupLayout-entry order regardless of the
    // caller-supplied `desc.ubos[]` shape).
    struct GLUBOBinding
    {
        unsigned int buffer = 0;
        uint32_t offset = 0;
        uint32_t size = 0;
        uint32_t binding = 0; // WGSL @binding — sort key for dynamicOffsets[].
        uint32_t slot = 0;    // GL binding point (allocator's global slot).
        // Populated from `BindGroupLayout::hasDynamicOffset(binding)` at
        // makeBindGroup time. setBindGroup consumes one `dynamicOffsets[]`
        // entry per `hasDynamicOffset=true` UBO in `binding`-ascending order.
        bool hasDynamicOffset = false;
    };
    struct GLTexBinding
    {
        unsigned int texture = 0;
        unsigned int target = 0;
        uint32_t slot = 0;
    };
    struct GLSamplerBinding
    {
        unsigned int sampler = 0;
        uint32_t slot = 0;
    };
    std::vector<GLUBOBinding> m_glUBOs;
    std::vector<GLTexBinding> m_glTextures;
    std::vector<GLSamplerBinding> m_glSamplers;
#endif
#if defined(ORE_BACKEND_D3D11)
    // D3D11: store resource pointers + per-stage register slots.
    //
    // D3D11 has independent VS / PS register namespaces (`b0` in VS is a
    // different register than `b0` in PS). The allocator (RFC §3.4) is
    // free to assign different per-stage register numbers — VS-only and
    // PS-only resources can share register 0 in their own stages. Each
    // `*Slot` is `BindingMap::kAbsent` when the binding isn't visible to
    // that stage; setBindGroup then skips the per-stage `VSSet*` /
    // `PSSet*` call so we don't overwrite another resource's slot.
    //
    // `binding` carries the WGSL `@binding` value, used as the sort key
    // for dynamic-offset ordering (same shape as the Metal binding) so
    // `dynamicOffsets[]` consumption matches BindGroupLayout-entry
    // order regardless of caller-supplied `desc.ubos[]` shape.
    //
    // `offset`/`size` are in bytes; applied via VSSetConstantBuffers1 /
    // PSSetConstantBuffers1 (which take firstConstant / numConstants in
    // 16-byte units — D3D11 requires 256-byte alignment ≡ 16 constants).
    struct D3D11UBOBinding
    {
        ID3D11Buffer* buffer = nullptr; // Weak ref; rcp held in
                                        // m_retainedBuffers.
        uint32_t binding = 0; // WGSL @binding — sort key for dynamicOffsets[].
        uint16_t vsSlot = 0xFFFF; // BindingMap::kAbsent.
        uint16_t psSlot = 0xFFFF;
        uint32_t offset = 0;
        uint32_t size = 0;
        // Populated from `BindGroupLayout::hasDynamicOffset(binding)` at
        // makeBindGroup time. setBindGroup consumes one `dynamicOffsets[]`
        // entry per `hasDynamicOffset=true` UBO in `binding`-ascending order.
        bool hasDynamicOffset = false;
    };
    struct D3D11TexBinding
    {
        ID3D11ShaderResourceView* srv = nullptr; // Weak ref.
        uint16_t vsSlot = 0xFFFF;
        uint16_t psSlot = 0xFFFF;
    };
    struct D3D11SamplerBinding
    {
        ID3D11SamplerState* sampler = nullptr; // Weak ref.
        uint16_t vsSlot = 0xFFFF;
        uint16_t psSlot = 0xFFFF;
    };
    std::vector<D3D11UBOBinding> m_d3d11UBOs;
    std::vector<D3D11TexBinding> m_d3d11Textures;
    std::vector<D3D11SamplerBinding> m_d3d11Samplers;
#endif

public:
    void onRefCntReachedZero() const;
};

} // namespace rive::ore
