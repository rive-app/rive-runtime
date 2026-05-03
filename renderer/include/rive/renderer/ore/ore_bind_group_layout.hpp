/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"

#include <string>
#include <vector>

// Only the backends that store a native handle on `BindGroupLayout` need
// their headers here. Metal / D3D11 / GL keep entries-only layouts (no
// native handle), so their backend headers are NOT pulled in — that lets
// the cross-backend `ore_bind_group_layout.cpp` compile as plain C++.
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

#if defined(ORE_BACKEND_D3D12)
// Per-group root-parameter metadata for D3D12. Moved from `Pipeline` (where
// it was historically derived from the binding map at pipeline build time)
// onto `BindGroupLayout` so multiple pipelines that share a layout produce
// the same per-group descriptor-table shape. The composing root signature
// is still per-pipeline (D3D12 root sigs are cross-group).
struct D3D12GroupRootParams
{
    int8_t cbvSrvUavRootParamIdx = -1;
    int8_t samplerRootParamIdx = -1;
    uint8_t cbvCount = 0;
    uint8_t srvCount = 0;
    uint8_t uavCount = 0;
    uint8_t samplerCount = 0;
    uint8_t cbvBaseGlobalSlot = 0;
    uint8_t srvBaseGlobalSlot = 0;
    uint8_t uavBaseGlobalSlot = 0;
    uint8_t samplerBaseGlobalSlot = 0;
};
#endif

// Public Ore type — created via `Context::makeBindGroupLayout`. Carries the
// user-supplied entries plus per-backend baked layout handles.
//
// Lifetime: outlives any `Pipeline` or `BindGroup` that references it.
// `Pipeline` holds `rcp<BindGroupLayout> m_layouts[kMaxBindGroups]`;
// `BindGroup` holds `rcp<BindGroupLayout> m_layoutRef`. Vulkan's
// `vkDestroyDescriptorSetLayout` runs only after the last consumer drops.
class BindGroupLayout : public RefCnt<BindGroupLayout>
{
public:
    uint32_t groupIndex() const { return m_groupIndex; }
    const std::vector<BindGroupLayoutEntry>& entries() const
    {
        return m_entries;
    }

#if defined(ORE_BACKEND_D3D12)
    // D3D12-only: per-group descriptor-table shape (counts + base global
    // slots). Read by `buildPerPipelineRootSig` to compose the root sig.
    const D3D12GroupRootParams& d3dGroupParams() const
    {
        return m_d3dGroupParams;
    }
#endif

    // True if entry for `binding` (within this layout's group) is a UBO
    // declared with `hasDynamicOffset = true`. Replaces
    // `Pipeline::isDynamicUBO()`. Each backend's BindGroup caches this at
    // makeBindGroup time per-slot to know when to consume from
    // `dynamicOffsets[]` at setBindGroup time.
    bool hasDynamicOffset(uint32_t binding) const;

    // Find the entry for a given binding. Returns nullptr if not present.
    const BindGroupLayoutEntry* findEntry(uint32_t binding) const;

private:
    friend class Context;

    BindGroupLayout() = default;

    uint32_t m_groupIndex = 0;
    std::vector<BindGroupLayoutEntry> m_entries;

    // Context back-pointer for deferred-destruction routing through
    // Context::vkDeferDestroy / d3dDeferDestroy. Weak ref.
    Context* m_context = nullptr;

#if defined(ORE_BACKEND_VK)
    VkDevice m_vkDevice = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_vkDSL = VK_NULL_HANDLE;
    PFN_vkDestroyDescriptorSetLayout m_vkDestroyDescriptorSetLayout = nullptr;
#endif
#if defined(ORE_BACKEND_WGPU)
    wgpu::BindGroupLayout m_wgpuBGL;
#endif
#if defined(ORE_BACKEND_D3D12)
    D3D12GroupRootParams m_d3dGroupParams;
#endif
    // Metal / D3D11 / GL / D3D12-bind-side: the user-supplied entries above
    // plus shader-resolved native slots per-stage are sufficient to drive
    // makeBindGroup for those backends. Slots are looked up via the
    // pipeline's BindingMap at makeBindGroup time and validated against
    // this layout at makePipeline time.

public:
    void onRefCntReachedZero() const;
};

// Validate user-supplied layouts against the shader's reflected binding map.
//
// Walks every entry in `bindingMap` and confirms the layout for that group
// declares a matching entry: same WGSL @binding, kind, visibility >=
// shader's stageMask, texture dim/sample type compatible.
//
// Returns true on success. On failure, populates `*outError` with a human-
// readable diagnostic (e.g. "@group(1) @binding(0): layout declares
// uniformBuffer but shader expects sampledTexture") and returns false. Never
// asserts.
//
// Called by every backend's `makePipeline` after the user-supplied layouts
// have been collected, before any backend-specific build work.
bool validateLayoutsAgainstBindingMap(const BindingMap& bindingMap,
                                      BindGroupLayout* const* layouts,
                                      uint32_t layoutCount,
                                      std::string* outError);

// Color outputs require a fragment shader. Mirrors Dawn's
// ValidateRenderPipelineDescriptor invariant. Depth-only pipelines
// (colorCount == 0) may legitimately omit the fragment module — the
// rasterizer still writes depth.
bool validateColorRequiresFragment(uint32_t colorCount,
                                   bool hasFragmentModule,
                                   std::string* outError);

} // namespace rive::ore
