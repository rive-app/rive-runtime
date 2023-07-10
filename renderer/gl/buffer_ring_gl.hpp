/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/pls/buffer_ring.hpp"
#include "rive/pls/gl/gles3.hpp"

namespace rive::pls
{
// BufferRingImpl in GL on a given buffer target. In order to support WebGL2, we don't do hardware
// mapping.
class BufferGL : public BufferRingShadowImpl
{
public:
    BufferGL(GLenum target, size_t capacity, size_t itemSizeInBytes);
    ~BufferGL() override;

    GLuint submittedBufferID() const { return m_ids[submittedBufferIdx()]; }

protected:
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) override;

private:
    const GLenum m_target;
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
    void submitTexels(int textureIdx, size_t updateWidthInTexels, size_t updateHeight) override;

private:
    const GLenum m_activeTextureIdx;
    GLuint m_ids[kBufferRingSize];
};
} // namespace rive::pls
