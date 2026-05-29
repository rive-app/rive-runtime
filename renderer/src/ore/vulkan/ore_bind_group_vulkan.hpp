#pragma once
#include "rive/renderer/ore/ore_bind_group.hpp"
#include <vulkan/vulkan.h>

namespace rive::ore
{
class ContextVulkan;

class BindGroupVulkan : public LITE_RTTI_OVERRIDE(BindGroup, BindGroupVulkan)
{
public:
    BindGroupVulkan() = default;
    ~BindGroupVulkan() override = default;
    // Deferred destruction is handled at the Lua GC level via
    // Context::deferBindGroupDestroy(). By the time refcount reaches zero,
    // the GPU is guaranteed to be done.

    void onRefCntReachedZero() const override;

private:
    friend class ContextVulkan;
    friend class RenderPassVulkan;
    VkDescriptorSet m_vkDescriptorSet = VK_NULL_HANDLE;
};
} // namespace rive::ore
