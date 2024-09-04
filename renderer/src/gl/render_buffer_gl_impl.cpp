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
    m_target(type == RenderBufferType::vertex ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER)
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
    for (GLuint bufferID : m_bufferIDs)
    {
        if (bufferID != 0)
        {
            m_state->deleteBuffer(bufferID);
        }
    }
}

void RenderBufferGLImpl::init(rcp<GLState> state)
{
    assert(!m_state);
    assert(!m_bufferIDs[0]);
    m_state = std::move(state);
    int bufferCount =
        (flags() & RenderBufferFlags::mappedOnceAtInitialization) ? 1 : gpu::kBufferRingSize;
    glGenBuffers(bufferCount, m_bufferIDs.data());
    m_state->bindVAO(0);
    for (int i = 0; i < bufferCount; ++i)
    {
        m_state->bindBuffer(m_target, m_bufferIDs[i]);
        glBufferData(m_target,
                     sizeInBytes(),
                     nullptr,
                     (flags() & RenderBufferFlags::mappedOnceAtInitialization) ? GL_STATIC_DRAW
                                                                               : GL_DYNAMIC_DRAW);
    }
}

std::array<GLuint, gpu::kBufferRingSize> RenderBufferGLImpl::detachBuffers()
{
    auto detachedBuffers = m_bufferIDs;
    m_bufferIDs.fill(0);
    return detachedBuffers;
}

void* RenderBufferGLImpl::onMap()
{
    if (!canMapBuffer())
    {
        if (!m_fallbackMappedMemory)
        {
            m_fallbackMappedMemory.reset(new uint8_t[sizeInBytes()]);
        }
        return m_fallbackMappedMemory.get();
    }
    else
    {
#ifndef RIVE_WEBGL
        m_state->bindVAO(0);
        m_state->bindBuffer(m_target, m_bufferIDs[backBufferIdx()]);
        return glMapBufferRange(m_target,
                                0,
                                sizeInBytes(),
                                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT |
                                    GL_MAP_UNSYNCHRONIZED_BIT);
#else
        // WebGL doesn't support buffer mapping.
        RIVE_UNREACHABLE();
#endif
    }
}

void RenderBufferGLImpl::onUnmap()
{
    m_state->bindVAO(0);
    m_state->bindBuffer(m_target, m_bufferIDs[backBufferIdx()]);
    if (!canMapBuffer())
    {
        glBufferSubData(m_target, 0, sizeInBytes(), m_fallbackMappedMemory.get());
        if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
        {
            m_fallbackMappedMemory.reset(); // This buffer will only be mapped once.
        }
    }
    else
    {
#ifndef RIVE_WEBGL
        glUnmapBuffer(m_target);
#else
        // WebGL doesn't support buffer mapping.
        RIVE_UNREACHABLE();
#endif
    }
}

bool RenderBufferGLImpl::canMapBuffer() const
{
#ifdef RIVE_WEBGL
    // WebGL doesn't support buffer mapping.
    return false;
#else
    // NVIDIA gives performance warnings when mapping GL_STATIC_DRAW buffers.
    return !(flags() & RenderBufferFlags::mappedOnceAtInitialization);
#endif
}
} // namespace rive::gpu
