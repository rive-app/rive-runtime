/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/pls/buffer_ring.hpp"
#include "rive/pls/gl/gles3.hpp"

namespace rive::pls
{
// BufferRingImpl of a GL_ARRAY_BUFFER. In order to support WebGL2, we don't support mapping.
class VertexBufferGL : public BufferRingShadowImpl
{
public:
    VertexBufferGL(size_t capacity, size_t itemSizeInBytes);
    ~VertexBufferGL() override;

    void bindCurrentBuffer() const;

protected:
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) override;

private:
    GLuint m_ids[kBufferRingSize];
};

// BufferRingImpl of a GL_UNIFORM_BUFFER.
class UniformBufferGL : public UniformBufferRing
{
public:
    UniformBufferGL(size_t itemSizeInBytes);
    ~UniformBufferGL() override;

    void bindToUniformBlock(GLuint blockIndex) const;

protected:
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) override;

private:
    GLuint m_ids[kBufferRingSize];
};

class TexelBufferGL : public TexelBufferRing
{
public:
    TexelBufferGL(Format,
                  size_t widthInItems,
                  size_t height,
                  size_t texelsPerItem,
                  GLenum activeTextureIdx,
                  Filter);
    ~TexelBufferGL() override;

    GLuint submittedTextureID() const { return m_ids[submittedBufferIdx()]; }

protected:
    void submitTexels(int bufferIdx, size_t width, size_t height) override;

private:
    const GLenum m_activeTextureIdx;
    GLuint m_ids[kBufferRingSize];
};
} // namespace rive::pls
