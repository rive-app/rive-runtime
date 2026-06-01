/*
 * Copyright 2025 Rive
 */

#include "ore_shader_module_vulkan.hpp"

namespace rive::ore
{

ShaderModuleVulkan::~ShaderModuleVulkan()
{
    if (m_vkShaderModule != VK_NULL_HANDLE &&
        m_vkDestroyShaderModule != nullptr)
    {
        m_vkDestroyShaderModule(m_vkDevice, m_vkShaderModule, nullptr);
    }
}

} // namespace rive::ore
