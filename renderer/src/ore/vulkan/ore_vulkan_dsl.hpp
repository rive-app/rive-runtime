/*
 * Copyright 2026 Rive
 *
 * Private helpers for building `VkDescriptorSetLayout`s from a public
 * `BindGroupLayoutDesc`. The user supplies entries up front via
 * `Context::makeBindGroupLayout`, and the DSL is built once per layout
 * instead of per pipeline.
 */

#pragma once

#include "rive/renderer/ore/ore_types.hpp" // BindGroupLayoutDesc, kMaxBindGroups

#include <vulkan/vulkan.h>
#include <cassert>
#include <cstdint>
#include <vector>

namespace rive::ore
{

constexpr uint32_t kVkMaxGroups = kMaxBindGroups;

// Bindings per group the descriptor pool budgets for. A wider group still
// builds; it only spends the budget faster.
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
// The descriptor `binding` must match what the SPIR-V emits, so it comes
// from the allocator-assigned native slot (`nativeSlotVS` / `nativeSlotFS`)
// that `makeLayoutFromShader` populates from the shader's binding map. That
// slot is the WGSL `@binding` as of allocator v2, but reading it from the
// map is what keeps this correct across allocator changes.
//
// Empty descs produce a valid empty DSL.
inline VkDescriptorSetLayout createDSLFromLayoutDesc(
    PFN_vkCreateDescriptorSetLayout pfnCreateDSL,
    VkDevice device,
    const BindGroupLayoutDesc& desc)
{
  const uint32_t n = desc.entryCount;
  std::vector<VkDescriptorSetLayoutBinding> bindings(n);

  for (uint32_t i = 0; i < n; ++i)
  {
    const BindGroupLayoutEntry& e = desc.entries[i];
    const bool dynamic = e.kind == BindingKind::uniformBuffer &&
                         e.hasDynamicOffset;
    VkDescriptorSetLayoutBinding& b = bindings[i];
    // Prefer pre-resolved native slot; fall back to FS for FS-only
    // resources; final fallback to WGSL @binding (identity for
    // shader paths that don't compact).
    const uint32_t kAbsent = BindGroupLayoutEntry::kNativeSlotAbsent;
    const uint32_t nativeBinding = (e.nativeSlotVS != kAbsent) ? e.nativeSlotVS
                                   : (e.nativeSlotFS != kAbsent)
                                       ? e.nativeSlotFS
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
  ci.pBindings = bindings.data();

  VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
  pfnCreateDSL(device, &ci, nullptr, &dsl);
  return dsl;
}

} // namespace rive::ore
