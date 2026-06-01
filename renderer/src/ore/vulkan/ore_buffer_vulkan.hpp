#pragma once
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include <vulkan/vulkan.h>
VK_DEFINE_HANDLE(VmaAllocation);

namespace rive::ore
{
class ContextVulkan;

class BufferVulkan : public LITE_RTTI_OVERRIDE(Buffer, BufferVulkan)
{
public:
    BufferVulkan(rcp<rive::gpu::GPUResourceManager> manager,
                 uint32_t size,
                 BufferUsage usage) :
        lite_rtti_override(std::move(manager), size, usage)
    {}
    ~BufferVulkan() override;
    void update(const void* data, uint32_t size, uint32_t offset) override;

private:
    friend class ContextVulkan;
    friend class RenderPassVulkan;
    friend class TextureVulkan; // for staging buffer access during texture
                                // uploads
    VkBuffer m_vkBuffer = VK_NULL_HANDLE;
    VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;
    void* m_vkMappedPtr = nullptr;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    rcp<rive::gpu::VulkanContext> m_vk;
};
} // namespace rive::ore
