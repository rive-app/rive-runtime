#pragma once
#include "rive/renderer/ore/ore_buffer.hpp"
#include <vulkan/vulkan.h>
VK_DEFINE_HANDLE(VmaAllocator);
VK_DEFINE_HANDLE(VmaAllocation);

namespace rive::ore
{
class ContextVulkan;

class BufferVulkan : public LITE_RTTI_OVERRIDE(Buffer, BufferVulkan)
{
public:
    BufferVulkan(uint32_t size, BufferUsage usage) :
        lite_rtti_override(size, usage)
    {}
    // Destructor defers GPU buffer destruction via ContextVulkan.
    ~BufferVulkan() override = default;
    void update(const void* data, uint32_t size, uint32_t offset) override;
    // Vulkan frees the CPU object immediately; GPU resource is deferred
    // via the destructor.
    void onRefCntReachedZero() const override;

private:
    friend class ContextVulkan;
    friend class RenderPassVulkan;
    VkBuffer m_vkBuffer = VK_NULL_HANDLE;
    VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;
    void* m_vkMappedPtr = nullptr;
    VkDevice m_vkDevice = VK_NULL_HANDLE;         // Weak ref.
    VmaAllocator m_vmaAllocator = VK_NULL_HANDLE; // Weak ref.
    // Back-ref so the destructor can route vmaDestroyBuffer through
    // ContextVulkan::vkDeferDestroy() in external-CB mode. Weak ref.
    ContextVulkan* m_vkOreContext = nullptr;
};
} // namespace rive::ore
