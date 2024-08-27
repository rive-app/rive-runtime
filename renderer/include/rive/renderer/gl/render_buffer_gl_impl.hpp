/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer.hpp"
#include "rive/renderer/gl/gles3.hpp"
#include "rive/renderer/gpu.hpp"
#include <array>

namespace rive::gpu
{
class GLState;

// OpenGL backend implementation of rive::RenderBuffer.
class PLSRenderBufferGLImpl : public lite_rtti_override<RenderBuffer, PLSRenderBufferGLImpl>
{
public:
    PLSRenderBufferGLImpl(RenderBufferType, RenderBufferFlags, size_t, rcp<GLState>);
    ~PLSRenderBufferGLImpl();

    GLuint submittedBufferID() const { return m_bufferIDs[m_submittedBufferIdx]; }

protected:
    PLSRenderBufferGLImpl(RenderBufferType type, RenderBufferFlags flags, size_t sizeInBytes);

    void init(rcp<GLState>);

    // Used by the android runtime to marshal buffers off to the GL thread for deletion.
    std::array<GLuint, gpu::kBufferRingSize> detachBuffers();

    void* onMap() override;
    void onUnmap() override;

    GLState* state() const { return m_state.get(); }

private:
    // Returns whether glMapBufferRange() is supported for our buffer. If not, we use
    // m_fallbackMappedMemory.
    bool canMapBuffer() const;

    const GLenum m_target;
    std::array<GLuint, gpu::kBufferRingSize> m_bufferIDs{};
    int m_submittedBufferIdx = -1;
    std::unique_ptr<uint8_t[]> m_fallbackMappedMemory; // Used when canMapBuffer() is false.
    rcp<GLState> m_state;
};
} // namespace rive::gpu
