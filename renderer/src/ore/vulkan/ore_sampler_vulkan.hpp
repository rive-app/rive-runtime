#pragma once
#include "rive/renderer/ore/ore_sampler.hpp"
#include <vulkan/vulkan.h>

namespace rive::ore
{
class ContextVulkan;

class SamplerVulkan : public LITE_RTTI_OVERRIDE(Sampler, SamplerVulkan)
{
public:
    SamplerVulkan(rcp<rive::gpu::GPUResourceManager> manager) :
        lite_rtti_override(std::move(manager))
    {}
    ~SamplerVulkan() override;

private:
    friend class ContextVulkan;
    friend class RenderPassVulkan;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    VkSampler m_vkSampler = VK_NULL_HANDLE;
    PFN_vkDestroySampler m_vkDestroySampler = nullptr;
};
} // namespace rive::ore
