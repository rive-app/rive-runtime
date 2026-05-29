/*
 * Copyright 2025 Rive
 */

#include "ore_buffer_vulkan.hpp"
#include "rive/renderer/ore/ore_context_vulkan.hpp"

#include <vk_mem_alloc.h>

#include <cassert>
#include <cstring>

namespace rive::ore
{

void BufferVulkan::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    assert(m_vkMappedPtr != nullptr);
    memcpy(static_cast<uint8_t*>(m_vkMappedPtr) + offset, data, size);
}

void BufferVulkan::onRefCntReachedZero() const
{
    VmaAllocator allocator = m_vmaAllocator;
    VkBuffer buf = m_vkBuffer;
    VmaAllocation alloc = m_vmaAllocation;
    ContextVulkan* ctx = m_vkOreContext;

    auto destroy = [=]() {
        if (buf != VK_NULL_HANDLE)
            vmaDestroyBuffer(allocator, buf, alloc);
    };

    delete this;

    if (ctx != nullptr)
        ctx->vkDeferDestroy(std::move(destroy));
    else
        destroy();
}

} // namespace rive::ore
