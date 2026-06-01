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
    ShaderModuleVulkan(rcp<rive::gpu::GPUResourceManager> manager) :
        lite_rtti_override(std::move(manager))
    {}
    ~ShaderModuleVulkan() override;

private:
    friend class ContextVulkan;
    friend class RenderPassVulkan;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    VkShaderModule m_vkShaderModule = VK_NULL_HANDLE;
    PFN_vkDestroyShaderModule m_vkDestroyShaderModule = nullptr;
};
} // namespace rive::ore
