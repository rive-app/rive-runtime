/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/rive_types.hpp"

#import <Metal/Metal.h>

namespace rive::ore
{

#if !defined(ORE_BACKEND_GL)

void Buffer::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    memcpy(static_cast<uint8_t*>([m_mtlBuffer contents]) + offset, data, size);
}

void Buffer::onRefCntReachedZero() const
{
    // Release the Metal buffer by destroying this object.
    delete this;
}

#endif // !ORE_BACKEND_GL

} // namespace rive::ore
