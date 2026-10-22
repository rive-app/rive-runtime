#pragma once
#include "rive/renderer/ore/ore_shader_module.hpp"
#include <vulkan/vulkan.h>

#include <vector>

namespace rive::gpu
{
class VulkanContext;
}

namespace rive::ore
{
class ContextVulkan;
struct SpirvEntryScratch;

VkResult createVkShaderModule(
    const rive::gpu::VulkanContext& vk,
    const uint32_t* code,
    size_t wordCount,
    VkShaderModule* out);

class ShaderModuleVulkan
    : public LITE_RTTI_OVERRIDE(ShaderModule, ShaderModuleVulkan)
{
public:
  ShaderModuleVulkan(rcp<rive::gpu::GPUResourceManager> manager)
      : lite_rtti_override(std::move(manager))
  {}
  ~ShaderModuleVulkan() override;

private:
  // Cut down to one entry point into entryModule, which the caller destroys
  // once its pipeline is built; the whole module when it cannot be cut.
  VkShaderModule stageModule(
      const rive::gpu::VulkanContext& vk,
      uint32_t executionModel,
      const char* entryPoint,
      SpirvEntryScratch& scratch,
      VkShaderModule* entryModule) const;

  friend class ContextVulkan;
  friend class RenderPassVulkan;
  VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
  VkShaderModule m_vkShaderModule = VK_NULL_HANDLE;
  PFN_vkDestroyShaderModule m_vkDestroyShaderModule = nullptr;
  // Kept when the module holds several entry points, for pipelines to cut
  // down to the ones they use.
  std::vector<uint32_t> m_multiEntryCode;
};
} // namespace rive::ore
