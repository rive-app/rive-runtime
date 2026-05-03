/*
 * Copyright 2025 Rive
 */

// Combined Metal + GL Buffer implementation for macOS.
// Compiled when both ORE_BACKEND_METAL and ORE_BACKEND_GL are active.

#if defined(ORE_BACKEND_METAL) && defined(ORE_BACKEND_GL)

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/rive_types.hpp"

#import <Metal/Metal.h>

namespace rive::ore
{

void Buffer::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    if (m_glBuffer != 0)
    {
        glBindBuffer(m_glTarget, m_glBuffer);
        glBufferSubData(m_glTarget, offset, size, data);
        glBindBuffer(m_glTarget, 0);
        return;
    }
    memcpy(static_cast<uint8_t*>([m_mtlBuffer contents]) + offset, data, size);
}

void Buffer::onRefCntReachedZero() const
{
    if (m_glBuffer != 0)
    {
        GLuint buf = m_glBuffer;
        glDeleteBuffers(1, &buf);
    }
    delete this;
}

} // namespace rive::ore

#endif // ORE_BACKEND_METAL && ORE_BACKEND_GL
