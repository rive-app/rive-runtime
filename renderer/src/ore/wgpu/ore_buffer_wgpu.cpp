/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/rive_types.hpp"

namespace rive::ore
{

void Buffer::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    assert(m_wgpuBuffer != nullptr);
    m_wgpuQueue.WriteBuffer(m_wgpuBuffer, offset, data, size);
}

void Buffer::onRefCntReachedZero() const
{
    // wgpu::Buffer RAII destructor releases the GPU resource.
    delete this;
}

} // namespace rive::ore
