#pragma once
#include "rive/renderer/ore/ore_shader_module.hpp"
#include <vulkan/vulkan.h>

namespace rive::ore
{
class ContextVulkan;

class ShaderModuleVulkan
    : public LITE_RTTI_OVERRIDE(ShaderModule, ShaderModuleVulkan)
{
public:
    ShaderModuleVulkan() = default;
    ~ShaderModuleVulkan() override = default;
    void onRefCntReachedZero() const override;

private:
    friend class ContextVulkan;
    friend class RenderPassVulkan;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    VkShaderModule m_vkShaderModule = VK_NULL_HANDLE;
    PFN_vkDestroyShaderModule m_vkDestroyShaderModule = nullptr;
};
} // namespace rive::ore
