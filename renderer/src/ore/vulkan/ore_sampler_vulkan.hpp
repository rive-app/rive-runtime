#pragma once
#include "rive/renderer/ore/ore_sampler.hpp"
#include <vulkan/vulkan.h>

namespace rive::ore
{
class ContextVulkan;

class SamplerVulkan : public LITE_RTTI_OVERRIDE(Sampler, SamplerVulkan)
{
public:
    SamplerVulkan() = default;
    ~SamplerVulkan() override = default;
    // Defers vkDestroySampler through ContextVulkan::vkDeferDestroy().
    void onRefCntReachedZero() const override;

private:
    friend class ContextVulkan;
    friend class RenderPassVulkan;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    VkSampler m_vkSampler = VK_NULL_HANDLE;
    PFN_vkDestroySampler m_vkDestroySampler = nullptr;
    // Back-ref for deferred destruction. Weak ref.
    ContextVulkan* m_vkOreContext = nullptr;
};
} // namespace rive::ore
