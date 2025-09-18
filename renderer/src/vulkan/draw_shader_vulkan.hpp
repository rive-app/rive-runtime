/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/gpu.hpp"
#include <vulkan/vulkan.h>

namespace rive::gpu
{
class VulkanContext;
}

namespace rive::gpu
{
class DrawShaderVulkan
{
public:
    enum class Type
    {
        vertex,
        fragment,
    };
    DrawShaderVulkan(Type,
                     VulkanContext*,
                     DrawType,
                     ShaderFeatures,
                     InterlockMode,
                     ShaderMiscFlags);

    ~DrawShaderVulkan();

    DrawShaderVulkan(const DrawShaderVulkan&) = delete;
    DrawShaderVulkan& operator=(const DrawShaderVulkan&) = delete;

    VkShaderModule module() const { return m_module; }

private:
    const rcp<VulkanContext> m_vk;
    VkShaderModule m_module = VK_NULL_HANDLE;
};
} // namespace rive::gpu