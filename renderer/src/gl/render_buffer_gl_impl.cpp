/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/gl/render_buffer_gl_impl.hpp"

#include "rive/renderer/gl/gl_state.hpp"

namespace rive::gpu
{
RenderBufferGLImpl::RenderBufferGLImpl(RenderBufferType type,
                                       RenderBufferFlags flags,
                                       size_t sizeInBytes) :
    lite_rtti_override(type, flags, sizeInBytes),
    m_target(type == RenderBufferType::vertex ? GL_ARRAY_BUFFER
                                              : GL_ELEMENT_ARRAY_BUFFER)
{}

RenderBufferGLImpl::RenderBufferGLImpl(RenderBufferType RenderBufferType,
                                       RenderBufferFlags renderBufferFlags,
                                       size_t sizeInBytes,
                                       rcp<GLState> state) :
    RenderBufferGLImpl(RenderBufferType, renderBufferFlags, sizeInBytes)
{
    init(std::move(state));
}

RenderBufferGLImpl::~RenderBufferGLImpl()
{
    if (m_bufferID != 0)
    {
        m_state->deleteBuffer(m_bufferID);
    }
}

void RenderBufferGLImpl::init(rcp<GLState> state)
{
    assert(!m_state);
    assert(m_bufferID == 0);
    m_state = std::move(state);
    glGenBuffers(1, &m_bufferID);
    m_state->bindVAO(0);
    m_state->bindBuffer(m_target, m_bufferID);
    glBufferData(m_target,
                 sizeInBytes(),
                 nullptr,
                 (flags() & RenderBufferFlags::mappedOnceAtInitialization)
                     ? GL_STATIC_DRAW
                     : GL_DYNAMIC_DRAW);
}

GLuint RenderBufferGLImpl::detachBuffer()
{
    return std::exchange(m_bufferID, 0);
}

void* RenderBufferGLImpl::onMap()
{
#ifndef RIVE_WEBGL
    if (canMapBuffer())
    {
        m_state->bindVAO(0);
        m_state->bindBuffer(m_target, m_bufferID);
        return glMapBufferRange(m_target,
                                0,
                                sizeInBytes(),
                                GL_MAP_WRITE_BIT |
                                    GL_MAP_INVALIDATE_BUFFER_BIT);
    }
    else
#endif
    {
        if (!m_fallbackMappedMemory)
        {
            m_fallbackMappedMemory.reset(new uint8_t[sizeInBytes()]);
        }
        return m_fallbackMappedMemory.get();
    }
}

void RenderBufferGLImpl::onUnmap()
{
    m_state->bindVAO(0);
    m_state->bindBuffer(m_target, m_bufferID);
#ifndef RIVE_WEBGL
    if (canMapBuffer())
    {
        glUnmapBuffer(m_target);
    }
    else
#endif
    {
        glBufferSubData(m_target,
                        0,
                        sizeInBytes(),
                        m_fallbackMappedMemory.get());
        if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
        {
            m_fallbackMappedMemory
                .reset(); // This buffer will only be mapped once.
        }
    }
}

#ifndef RIVE_WEBGL
bool RenderBufferGLImpl::canMapBuffer() const
{
    // WebGL doesn't support buffer mapping. Don't use it on ANGLE either since
    // we don't trust ANGLE with features that haven't been validated by WebGL.
    return !m_state->capabilities().isANGLESystemDriver &&
           // NVIDIA gives performance warnings when mapping GL_STATIC_DRAW
           // buffers.
           !(flags() & RenderBufferFlags::mappedOnceAtInitialization);
}
#endif
} // namespace rive::gpu
