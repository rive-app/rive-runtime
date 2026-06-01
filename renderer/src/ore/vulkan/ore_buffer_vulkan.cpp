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
    if (m_vkBuffer != VK_NULL_HANDLE)
        vmaDestroyBuffer(m_vk->allocator(), m_vkBuffer, m_vmaAllocation);
}

void BufferVulkan::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    assert(m_vkMappedPtr != nullptr);
    memcpy(static_cast<uint8_t*>(m_vkMappedPtr) + offset, data, size);
}

} // namespace rive::ore
