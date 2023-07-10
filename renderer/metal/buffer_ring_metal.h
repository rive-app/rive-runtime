/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/buffer_ring.hpp"

#ifndef RIVE_OBJC_NOP
#import <Metal/Metal.h>
#endif

namespace rive::pls
{
class BufferMetal : public BufferRingImpl
{
public:
    BufferMetal(id<MTLDevice>, size_t capacity, size_t itemSizeInBytes);
    ~BufferMetal() override {}

    id<MTLBuffer> submittedBuffer() const { return m_buffers[submittedBufferIdx()]; }

protected:
    void* onMapBuffer(int bufferIdx) override;
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) override {}

private:
    id<MTLBuffer> m_buffers[kBufferRingSize];
};

class TexelBufferMetal : public TexelBufferRing
{
public:
    TexelBufferMetal(id<MTLDevice>,
                     Format,
                     size_t widthInItems,
                     size_t height,
                     size_t texelsPerItem,
                     MTLTextureUsage extraUsageFlags = 0);
    ~TexelBufferMetal() override {}

    id<MTLTexture> submittedTexture() const { return m_textures[submittedBufferIdx()]; }

protected:
    void submitTexels(int textureIdx, size_t updateWidthInTexels, size_t updateHeight) override;

private:
    id<MTLTexture> m_textures[kBufferRingSize];
};
} // namespace rive::pls
