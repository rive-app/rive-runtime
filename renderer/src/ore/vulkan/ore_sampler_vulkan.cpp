/*
 * Copyright 2025 Rive
 */

#include "ore_sampler_vulkan.hpp"

namespace rive::ore
{

SamplerVulkan::~SamplerVulkan()
{
    if (m_vkSampler != VK_NULL_HANDLE && m_vkDestroySampler != nullptr)
        m_vkDestroySampler(m_vkDevice, m_vkSampler, nullptr);
}

} // namespace rive::ore
