/*
 * Copyright 2025 Rive
 */

#include "ore_buffer_metal.hpp"
#include "rive/rive_types.hpp"

#import <Metal/Metal.h>

namespace rive::ore
{

void BufferMetal::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    memcpy(static_cast<uint8_t*>([m_mtlBuffer contents]) + offset, data, size);
}

} // namespace rive::ore
