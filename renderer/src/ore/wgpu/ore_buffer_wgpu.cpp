/*
 * Copyright 2025 Rive
 */

#include "ore_buffer_wgpu.hpp"
#include "rive/rive_types.hpp"

namespace rive::ore
{

void BufferWGPU::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    assert(m_wgpuBuffer != nullptr);
    m_wgpuQueue.WriteBuffer(m_wgpuBuffer, offset, data, size);
}

} // namespace rive::ore
