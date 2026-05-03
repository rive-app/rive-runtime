/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_context.hpp"

#include <vk_mem_alloc.h>

#include <cassert>
#include <cstring>

namespace rive::ore
{

#if !defined(ORE_BACKEND_GL)

void Buffer::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    assert(m_vkMappedPtr != nullptr);
    memcpy(static_cast<uint8_t*>(m_vkMappedPtr) + offset, data, size);
}

void Buffer::onRefCntReachedZero() const
{
    VmaAllocator allocator = m_vmaAllocator;
    VkBuffer buf = m_vkBuffer;
    VmaAllocation alloc = m_vmaAllocation;
    Context* ctx = m_vkOreContext;

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

#endif // !ORE_BACKEND_GL
} // namespace rive::ore
