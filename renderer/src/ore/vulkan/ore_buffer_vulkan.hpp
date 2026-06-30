#pragma once
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include <vector>
#include <vulkan/vulkan.h>
VK_DEFINE_HANDLE(VmaAllocation);

namespace rive::ore
{
class ContextVulkan;

// An update after a bind orphans onto a fresh backing instead of racing the GPU
// still reading the bound one. Never rebound buffers stay at one backing.
// Reclamation uses the manager frame numbers.
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

    // Backing to bind right now.
    VkBuffer current() const { return m_vkBuffer; }

    // Tag the current backing with this frame's number so a later update()
    // orphans instead of overwriting in-flight memory.
    void markBound();

private:
    friend class ContextVulkan;
    friend class RenderPassVulkan;
    friend class TextureVulkan; // for staging buffer access during texture
                                // uploads

    // The current backing is mirrored into m_vkBuffer, m_vmaAllocation and
    // m_vkMappedPtr so existing direct readers like texture staging and the
    // descriptor copy region always see it.
    struct Backing
    {
        VkBuffer vkBuffer = VK_NULL_HANDLE;
        VmaAllocation vmaAllocation = VK_NULL_HANDLE;
        void* mappedPtr = nullptr;
        uint64_t frameStamp = 0; // frame number it was last bound in
    };

    // Move to a fresh backing, reusing one the GPU has retired or allocating
    // one. Copies current contents forward so a partial update keeps untouched
    // bytes; the pending write's range lets a full-buffer update skip the copy.
    // Returns false if a fresh backing could not be allocated, in which case
    // the current backing is kept.
    bool acquireFreshBacking(uint32_t writeOffset, uint32_t writeSize);
    Backing allocateBacking();

    // Current backing, mirrored for direct readers. Staging buffers created by
    // TextureVulkan set these directly and leave m_pool empty.
    VkBuffer m_vkBuffer = VK_NULL_HANDLE;
    VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;
    void* m_vkMappedPtr = nullptr;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    rcp<rive::gpu::VulkanContext> m_vk;

    // Backing pool for orphan on bound. Empty for staging buffers, which are
    // never bound. Bounded across frames by frames in flight, one extra per
    // write after bind.
    std::vector<Backing> m_pool;
    size_t m_currentIndex = 0;
    bool m_boundSinceUpdate = false;
    VkBufferUsageFlags m_vkUsage = 0;
};
} // namespace rive::ore
