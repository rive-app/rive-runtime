/*
 * Copyright 2023 Rive
 */

#include "buffer_ring_metal.h"

#include "rive/pls/metal/pls_render_context_metal.h"

namespace rive::pls
{
VertexBufferMetal::VertexBufferMetal(id<MTLDevice> gpu, size_t capacity, size_t itemSizeInBytes) :
    BufferRingImpl(capacity, itemSizeInBytes)
{
    for (int i = 0; i < kBufferRingSize; ++i)
    {
        m_buffers[i] = [gpu newBufferWithLength:capacity * itemSizeInBytes
                                        options:MTLResourceStorageModeShared];
    }
}

void* VertexBufferMetal::onMapBuffer(int bufferIdx) { return m_buffers[bufferIdx].contents; }

UniformBufferMetal::UniformBufferMetal(id<MTLDevice> gpu, size_t itemSizeInBytes) :
    UniformBufferRing(itemSizeInBytes)
{
    for (int i = 0; i < kBufferRingSize; ++i)
    {
        m_buffers[i] = [gpu newBufferWithLength:itemSizeInBytes
                                        options:MTLResourceStorageModeShared];
    }
}

void UniformBufferMetal::onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten)
{
    assert(bytesWritten == itemSizeInBytes());
    memcpy(m_buffers[bufferIdx].contents, shadowBuffer(), bytesWritten);
}

TexelBufferMetal::TexelBufferMetal(id<MTLDevice> gpu,
                                   Format format,
                                   size_t widthInItems,
                                   size_t height,
                                   size_t texelsPerItem,
                                   MTLTextureUsage extraUsageFlags) :
    TexelBufferRing(format, widthInItems, height, texelsPerItem)
{
    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    switch (format)
    {
        case Format::rgba8:
            desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
            break;
        case Format::rgba32f:
            desc.pixelFormat = MTLPixelFormatRGBA32Float;
            break;
        case Format::rgba32ui:
            desc.pixelFormat = MTLPixelFormatRGBA32Uint;
            break;
    }
    desc.width = widthInItems * texelsPerItem;
    desc.height = height;
    desc.mipmapLevelCount = 1;
    desc.usage = MTLTextureUsageShaderRead | extraUsageFlags;
    desc.storageMode = MTLStorageModeShared;
    desc.textureType = MTLTextureType2D;
    for (int i = 0; i < kBufferRingSize; ++i)
    {
        m_textures[i] = [gpu newTextureWithDescriptor:desc];
    }
}

void TexelBufferMetal::submitTexels(int textureIdx, size_t updateWidthInTexels, size_t updateHeight)
{
    if (updateWidthInTexels > 0 && updateHeight > 0)
    {
        MTLRegion region = {MTLOriginMake(0, 0, 0),
                            MTLSizeMake(updateWidthInTexels, updateHeight, 1)};
        [m_textures[textureIdx] replaceRegion:region
                                  mipmapLevel:0
                                    withBytes:shadowBuffer()
                                  bytesPerRow:widthInTexels() * BytesPerPixel(m_format)];
    }
}
} // namespace rive::pls
