#pragma once
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include <vulkan/vulkan.h>

namespace rive::ore
{
class ContextVulkan;

class BindGroupLayoutVulkan
    : public LITE_RTTI_OVERRIDE(BindGroupLayout, BindGroupLayoutVulkan)
{
public:
    BindGroupLayoutVulkan() = default;
    ~BindGroupLayoutVulkan() override = default;
    // Defers vkDestroyDescriptorSetLayout through
    // ContextVulkan::vkDeferDestroy().
    void onRefCntReachedZero() const override;

private:
    friend class ContextVulkan;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    VkDescriptorSetLayout m_vkDSL = VK_NULL_HANDLE;
    PFN_vkDestroyDescriptorSetLayout m_vkDestroyDescriptorSetLayout = nullptr;
};
} // namespace rive::ore
