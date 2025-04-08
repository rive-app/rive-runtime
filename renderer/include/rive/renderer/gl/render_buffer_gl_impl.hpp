/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/rive_render_buffer.hpp"
#include "rive/renderer/gl/gles3.hpp"
#include "rive/renderer/gpu.hpp"
#include <array>

namespace rive::gpu
{
class GLState;

// OpenGL backend implementation of rive::RenderBuffer.
class RenderBufferGLImpl
    : public LITE_RTTI_OVERRIDE(RiveRenderBuffer, RenderBufferGLImpl)
{
public:
    RenderBufferGLImpl(RenderBufferType,
                       RenderBufferFlags,
                       size_t,
                       rcp<GLState>);
    ~RenderBufferGLImpl();

    // Returns the buffer to submit to GL draw calls, updating it if dirty.
    GLuint bufferID() { return m_bufferID; }

protected:
    RenderBufferGLImpl(RenderBufferType type,
                       RenderBufferFlags flags,
                       size_t sizeInBytes);

    void init(rcp<GLState>);

    // Used by the android runtime to marshal the buffer off to the GL thread
    // for deletion.
    GLuint detachBuffer();

    void* onMap() override;
    void onUnmap() override;

    GLState* state() const { return m_state.get(); }

private:
    // Returns whether glMapBufferRange() is supported for our buffer. If not,
    // we use m_fallbackMappedMemory.
    bool canMapBuffer() const;

    const GLenum m_target;
    GLuint m_bufferID = 0;
    // Used when canMapBuffer() is false.
    std::unique_ptr<uint8_t[]> m_fallbackMappedMemory;
    rcp<GLState> m_state;
};
} // namespace rive::gpu
