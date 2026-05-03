/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/rive_types.hpp"

namespace rive::ore
{

#if defined(ORE_BACKEND_GL) && !defined(ORE_BACKEND_METAL) &&                  \
    !defined(ORE_BACKEND_VK)

void Buffer::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    assert(m_glBuffer != 0);

    if (m_usage == BufferUsage::index)
    {
        // WebGL forbids binding a buffer to GL_ELEMENT_ARRAY_BUFFER if it was
        // ever bound to a different target, so index buffers must use their
        // native target. GL_ELEMENT_ARRAY_BUFFER is VAO state, so we
        // save/restore the current binding to avoid clobbering the host
        // renderer's VAO.
        GLint prevEBO;
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prevEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_glBuffer);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, size, data);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevEBO);
    }
    else
    {
        // Non-index buffers use GL_COPY_WRITE_BUFFER to avoid disturbing
        // the host renderer's VAO element-buffer binding.
        glBindBuffer(GL_COPY_WRITE_BUFFER, m_glBuffer);
        glBufferSubData(GL_COPY_WRITE_BUFFER, offset, size, data);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }
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

#endif // ORE_BACKEND_GL && !ORE_BACKEND_METAL

} // namespace rive::ore
