/*
 * Copyright 2025 Rive
 */

#include "ore_buffer_vulkan.hpp"

#include <vk_mem_alloc.h>

#include <cassert>
#include <cstring>
#include <iostream>
namespace rive::ore
{

BufferVulkan::~BufferVulkan()
{
    if (!m_pool.empty())
    {
        // Pooled buffer. m_vkBuffer aliases the current pool entry, so free the
        // pool rather than m_vkBuffer separately to avoid a double free.
        for (auto& b : m_pool)
            vmaDestroyBuffer(m_vk->allocator(), b.vkBuffer, b.vmaAllocation);
    }
    else if (m_vkBuffer != VK_NULL_HANDLE)
    {
        // Staging buffer. A single allocation set directly by TextureVulkan.
        vmaDestroyBuffer(m_vk->allocator(), m_vkBuffer, m_vmaAllocation);
    }
}

BufferVulkan::Backing BufferVulkan::allocateBacking()
{
    VkBufferCreateInfo bufCI{};
    bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.size = m_size;
    bufCI.usage = m_vkUsage;

    // Host visible and persistently mapped so CPU writes go direct, no staging.
    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocInfo{};
    Backing b{};
    if (vmaCreateBuffer(m_vk->allocator(),
                        &bufCI,
                        &allocCI,
                        &b.vkBuffer,
                        &b.vmaAllocation,
                        &allocInfo) != VK_SUCCESS)
        return {}; // Null backing; caller keeps the current one.
    b.mappedPtr = allocInfo.pMappedData;
    return b;
}

void BufferVulkan::markBound()
{
    if (!m_pool.empty())
        m_pool[m_currentIndex].frameStamp = m_manager->currentFrameNumber();
    m_boundSinceUpdate = true;
}

bool BufferVulkan::acquireFreshBacking(uint32_t writeOffset, uint32_t writeSize)
{
    const uint64_t safe = m_manager->safeFrameNumber();
    const uint64_t current = m_manager->currentFrameNumber();
    void* oldMapped = m_pool[m_currentIndex].mappedPtr;

    // Reuse a backing the GPU has retired, else allocate one. The current
    // backing is never a candidate since it is the one we are orphaning from.
    // Require the bind to predate the current frame, not just safeFrameNumber,
    // so a host that ever reports safe == current cannot hand back an in flight
    // backing.
    size_t fresh = m_pool.size();
    for (size_t i = 0; i < m_pool.size(); ++i)
    {
        if (i != m_currentIndex && m_pool[i].frameStamp <= safe &&
            m_pool[i].frameStamp < current)
        {
            fresh = i;
            break;
        }
    }
    if (fresh == m_pool.size())
    {
        Backing nb = allocateBacking();
        if (nb.vkBuffer == VK_NULL_HANDLE)
            return false; // Allocation failed; keep the current backing.
        m_pool.push_back(nb);
    }

    m_currentIndex = fresh;
    Backing& b = m_pool[m_currentIndex];
    // Carry contents forward so a partial update keeps untouched bytes. Skip
    // when this update covers the whole buffer.
    if (!(writeOffset == 0 && writeSize == m_size))
        memcpy(b.mappedPtr, oldMapped, m_size);

    // Mirror for direct readers.
    m_vkBuffer = b.vkBuffer;
    m_vmaAllocation = b.vmaAllocation;
    m_vkMappedPtr = b.mappedPtr;
    return true;
}

void BufferVulkan::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    if (m_boundSinceUpdate && !m_pool.empty())
    {
        // On allocation failure keep the current backing and retry next update.
        if (acquireFreshBacking(offset, size))
            m_boundSinceUpdate = false;
    }
    assert(m_vkMappedPtr != nullptr);
    memcpy(static_cast<uint8_t*>(m_vkMappedPtr) + offset, data, size);
}

} // namespace rive::ore
