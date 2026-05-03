/*
 * Copyright 2026 Rive
 *
 * Private helpers for building `VkDescriptorSetLayout`s from a public
 * `BindGroupLayoutDesc` (RFC §9.1, Phase E — explicit BindGroupLayout
 * decoupling). Pre-Phase-E this file built DSLs from the pipeline's
 * `ore::BindingMap` directly; with Phase E the user supplies entries
 * up front via `Context::makeBindGroupLayout`, and the DSL is built
 * once per layout instead of per pipeline.
 */

#pragma once

#include "rive/renderer/ore/ore_types.hpp" // BindGroupLayoutDesc, kMaxBindGroups

#include <vulkan/vulkan.h>
#include <cassert>
#include <cstdint>

namespace rive::ore
{

constexpr uint32_t kVkMaxGroups = kMaxBindGroups;

// Upper bound on bindings per group. WebGPU's default
// `maxBindingsPerBindGroup` is 1000, but Ore doesn't need that much
// headroom — a generous fixed ceiling keeps the pipeline-create path
// off the heap. If we ever blow past this, bump it.
constexpr uint32_t kVkMaxBindingsPerGroup = 16;

inline VkDescriptorType oreBindingKindToVk(BindingKind kind, bool dynamic)
{
    switch (kind)
    {
        case BindingKind::uniformBuffer:
            return dynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                           : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case BindingKind::storageBufferRO:
        case BindingKind::storageBufferRW:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case BindingKind::sampledTexture:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case BindingKind::storageTexture:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case BindingKind::sampler:
        case BindingKind::comparisonSampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
    }
    assert(false && "oreBindingKindToVk: unhandled BindingKind");
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

inline VkShaderStageFlags oreVisibilityToVk(StageVisibility v)
{
    VkShaderStageFlags out = 0;
    if (v.mask & StageVisibility::kVertex)
        out |= VK_SHADER_STAGE_VERTEX_BIT;
    if (v.mask & StageVisibility::kFragment)
        out |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (v.mask & StageVisibility::kCompute)
        out |= VK_SHADER_STAGE_COMPUTE_BIT;
    return out;
}

// Build a `VkDescriptorSetLayout` from a public `BindGroupLayoutDesc`.
// One entry → one VkDescriptorSetLayoutBinding.
//
// The SPIR-V backend COMPACTS sparse `@binding`s within a group
// (`@binding(0,7)` → slots `0,1`). The descriptor `binding` here must
// match what the SPIR-V emits, which is the allocator-assigned native
// slot (`nativeSlotVS` / `nativeSlotFS`) — NOT the raw WGSL `@binding`.
// `makeLayoutFromShader` populates these from the shader's binding map.
//
// Empty descs produce a valid empty DSL.
inline VkDescriptorSetLayout createDSLFromLayoutDesc(
    PFN_vkCreateDescriptorSetLayout pfnCreateDSL,
    VkDevice device,
    const BindGroupLayoutDesc& desc)
{
    VkDescriptorSetLayoutBinding bindings[kVkMaxBindingsPerGroup];
    const uint32_t n = std::min(desc.entryCount, kVkMaxBindingsPerGroup);

    for (uint32_t i = 0; i < n; ++i)
    {
        const BindGroupLayoutEntry& e = desc.entries[i];
        const bool dynamic =
            e.kind == BindingKind::uniformBuffer && e.hasDynamicOffset;
        VkDescriptorSetLayoutBinding& b = bindings[i];
        // Prefer pre-resolved native slot; fall back to FS for FS-only
        // resources; final fallback to WGSL @binding (identity for
        // shader paths that don't compact).
        const uint32_t kAbsent = BindGroupLayoutEntry::kNativeSlotAbsent;
        const uint32_t nativeBinding =
            (e.nativeSlotVS != kAbsent)   ? e.nativeSlotVS
            : (e.nativeSlotFS != kAbsent) ? e.nativeSlotFS
                                          : e.binding;
        b.binding = nativeBinding;
        b.descriptorType = oreBindingKindToVk(e.kind, dynamic);
        b.descriptorCount = 1;
        b.stageFlags = oreVisibilityToVk(e.visibility);
        b.pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = n;
    ci.pBindings = bindings;

    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    pfnCreateDSL(device, &ci, nullptr, &dsl);
    return dsl;
}

} // namespace rive::ore
