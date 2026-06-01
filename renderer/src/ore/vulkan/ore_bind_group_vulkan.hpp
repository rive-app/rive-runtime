#pragma once
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_context_vulkan.hpp"

namespace rive::ore
{

class BindGroupVulkan : public LITE_RTTI_OVERRIDE(BindGroup, BindGroupVulkan)
{
public:
    BindGroupVulkan(rcp<rive::gpu::GPUResourceManager> manager) :
        lite_rtti_override(std::move(manager))
    {}
    ~BindGroupVulkan() override;

private:
    friend class ContextVulkan;
    friend class RenderPassVulkan;
    VkDescriptorSet m_vkDescriptorSet = VK_NULL_HANDLE;
    // Keeps the pool alive while this set is outstanding.
    rcp<DescriptorPoolGeneration> m_pool;
};
} // namespace rive::ore
