/*
 * Copyright 2025 Rive
 */

#include "ore_shader_module_vulkan.hpp"
#include "ore_spirv_entry_split.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"

#include <cstdio>

namespace rive::ore
{

ShaderModuleVulkan::~ShaderModuleVulkan()
{
  if (m_vkShaderModule != VK_NULL_HANDLE && m_vkDestroyShaderModule != nullptr)
  {
    m_vkDestroyShaderModule(m_vkDevice, m_vkShaderModule, nullptr);
  }
}

VkResult createVkShaderModule(
    const rive::gpu::VulkanContext& vk,
    const uint32_t* code,
    size_t wordCount,
    VkShaderModule* out)
{
  VkShaderModuleCreateInfo smCI{};
  smCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  smCI.codeSize = wordCount * sizeof(uint32_t);
  smCI.pCode = code;
  return vk.CreateShaderModule(vk.device, &smCI, nullptr, out);
}

VkShaderModule ShaderModuleVulkan::stageModule(
    const rive::gpu::VulkanContext& vk,
    uint32_t executionModel,
    const char* entryPoint,
    SpirvEntryScratch& scratch,
    VkShaderModule* entryModule) const
{
  *entryModule = VK_NULL_HANDLE;
  if (m_multiEntryCode.empty())
  {
    return m_vkShaderModule;
  }
  if (spirvEntryModule(
          m_multiEntryCode.data(),
          m_multiEntryCode.size(),
          executionModel,
          entryPoint,
          scratch) &&
      createVkShaderModule(
          vk,
          scratch.words.data(),
          scratch.words.size(),
          entryModule) == VK_SUCCESS)
  {
    return *entryModule;
  }
  *entryModule = VK_NULL_HANDLE;
  fprintf(
      stderr,
      "Ore Vulkan: '%s' uses its multi entry module whole\n",
      entryPoint != nullptr ? entryPoint : "");
  return m_vkShaderModule;
}

} // namespace rive::ore
