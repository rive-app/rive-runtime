/*
 * Copyright 2025 Rive
 */

#include "ore_shader_module_vulkan.hpp"
#include "rive/renderer/ore/ore_context_vulkan.hpp"

namespace rive::ore
{

void ShaderModuleVulkan::onRefCntReachedZero() const
{
    if (m_vkShaderModule != VK_NULL_HANDLE &&
        m_vkDestroyShaderModule != nullptr)
    {
        m_vkDestroyShaderModule(m_vkDevice, m_vkShaderModule, nullptr);
    }
    delete this;
}

} // namespace rive::ore
