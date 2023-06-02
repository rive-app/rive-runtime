/*
 * Copyright 2022 Rive
 */

#include "buffer_ring_gl.hpp"

#include <algorithm>

namespace rive::pls
{
VertexBufferGL::VertexBufferGL(size_t capacity, size_t itemSizeInBytes) :
    BufferRingShadowImpl(capacity, itemSizeInBytes)
{
    glGenBuffers(kBufferRingSize, m_ids);
    for (int i = 0; i < kBufferRingSize; ++i)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_ids[i]);
        glBufferData(GL_ARRAY_BUFFER, capacity * itemSizeInBytes, nullptr, GL_DYNAMIC_DRAW);
    }
}

VertexBufferGL::~VertexBufferGL() { glDeleteBuffers(kBufferRingSize, m_ids); }

void VertexBufferGL::bindCurrentBuffer() const
{
    glBindBuffer(GL_ARRAY_BUFFER, m_ids[submittedBufferIdx()]);
}

void VertexBufferGL::onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten)
{
    glBindBuffer(GL_UNIFORM_BUFFER, m_ids[bufferIdx]);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, bytesWritten, shadowBuffer());
}

UniformBufferGL::UniformBufferGL(size_t itemSizeInBytes) : UniformBufferRing(itemSizeInBytes)
{
    glGenBuffers(kBufferRingSize, m_ids);
    for (int i = 0; i < kBufferRingSize; ++i)
    {
        glBindBuffer(GL_UNIFORM_BUFFER, m_ids[i]);
        glBufferData(GL_UNIFORM_BUFFER, itemSizeInBytes, nullptr, GL_DYNAMIC_DRAW);
    }
}

UniformBufferGL::~UniformBufferGL() { glDeleteBuffers(kBufferRingSize, m_ids); }

void UniformBufferGL::bindToUniformBlock(GLuint blockIndex) const
{
    glBindBufferBase(GL_UNIFORM_BUFFER, blockIndex, m_ids[submittedBufferIdx()]);
}

void UniformBufferGL::onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten)
{
    assert(bytesWritten == itemSizeInBytes());
    glBindBuffer(GL_UNIFORM_BUFFER, m_ids[bufferIdx]);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, bytesWritten, shadowBuffer());
}

static GLenum internalformat_gl(TexelBufferRing::Format format)
{
    switch (format)
    {
        case TexelBufferRing::Format::rgba8:
            return GL_RGBA8;
        case TexelBufferRing::Format::rgba32f:
            return GL_RGBA32F;
        case TexelBufferRing::Format::rgba32ui:
            return GL_RGBA32UI;
    }
    RIVE_UNREACHABLE();
}

static GLenum format_gl(TexelBufferRing::Format format)
{
    switch (format)
    {
        case TexelBufferRing::Format::rgba8:
        case TexelBufferRing::Format::rgba32f:
            return GL_RGBA;
        case TexelBufferRing::Format::rgba32ui:
            return GL_RGBA_INTEGER;
    }
    RIVE_UNREACHABLE();
}

static GLenum type_gl(TexelBufferRing::Format format)
{
    switch (format)
    {
        case TexelBufferRing::Format::rgba8:
            return GL_UNSIGNED_BYTE;
        case TexelBufferRing::Format::rgba32f:
            return GL_FLOAT;
        case TexelBufferRing::Format::rgba32ui:
            return GL_UNSIGNED_INT;
    }
    RIVE_UNREACHABLE();
}

static GLenum filter_gl(TexelBufferRing::Filter filter)
{
    switch (filter)
    {
        case TexelBufferRing::Filter::nearest:
            return GL_NEAREST;
        case TexelBufferRing::Filter::linear:
            return GL_LINEAR;
    }
    RIVE_UNREACHABLE();
}

TexelBufferGL::TexelBufferGL(Format format,
                             size_t widthInItems,
                             size_t height,
                             size_t texelsPerItem,
                             GLenum activeTextureIdx,
                             Filter filter) :
    TexelBufferRing(format, widthInItems, height, texelsPerItem),
    m_activeTextureIdx(activeTextureIdx)
{
    GLenum filterGL = filter_gl(filter);
    glGenTextures(kBufferRingSize, m_ids);
    glActiveTexture(activeTextureIdx);
    for (int i = 0; i < kBufferRingSize; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, m_ids[i]);
        glTexStorage2D(GL_TEXTURE_2D, 1, internalformat_gl(m_format), widthInTexels(), m_height);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterGL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterGL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

TexelBufferGL::~TexelBufferGL() { glDeleteTextures(kBufferRingSize, m_ids); }

void TexelBufferGL::submitTexels(int textureIdx, size_t updateWidthInTexels, size_t updateHeight)
{
    glActiveTexture(m_activeTextureIdx);
    glBindTexture(GL_TEXTURE_2D, m_ids[textureIdx]);
    if (updateWidthInTexels > 0 && updateHeight > 0)
    {
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        updateWidthInTexels,
                        updateHeight,
                        format_gl(m_format),
                        type_gl(m_format),
                        shadowBuffer());
    }
}
} // namespace rive::pls
