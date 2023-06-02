/*
 * Copyright 2023 Rive
 */

#include "rive/pls/metal/pls_render_target_metal.h"

#include "rive/pls/pls.hpp"

namespace rive::pls
{
// All PLS planes besides the main framebuffer can exist in ephemeral "memoryless" storage. This
// means their contents are never actually written to main memory, and they only exist in fast tiled
// memory.
static id<MTLTexture> make_memoryless_pls_texture(id<MTLDevice> gpu,
                                                  MTLPixelFormat pixelFormat,
                                                  size_t width,
                                                  size_t height)
{
    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.pixelFormat = pixelFormat;
    desc.width = width;
    desc.height = height;
    desc.usage = MTLTextureUsageRenderTarget;
    desc.textureType = MTLTextureType2D;
    desc.mipmapLevelCount = 1;
    desc.storageMode = MTLStorageModeMemoryless;
    return [gpu newTextureWithDescriptor:desc];
}

PLSRenderTargetMetal::PLSRenderTargetMetal(id<MTLDevice> gpu,
                                           MTLPixelFormat pixelFormat,
                                           size_t width,
                                           size_t height,
                                           const PlatformFeatures& platformFeatures) :
    PLSRenderTarget(width, height), m_pixelFormat(pixelFormat)
{
    m_targetTexture = nil; // Will be configured later by setTargetTexture().
    m_coverageMemorylessTexture =
        make_memoryless_pls_texture(gpu, MTLPixelFormatRG16Float, width, height);
    m_originalDstColorMemorylessTexture =
        make_memoryless_pls_texture(gpu, m_pixelFormat, width, height);
    m_clipMemorylessTexture =
        make_memoryless_pls_texture(gpu, MTLPixelFormatRG16Float, width, height);
}

void PLSRenderTargetMetal::setTargetTexture(id<MTLTexture> texture)
{
    assert(texture.width == width());
    assert(texture.height == height());
    assert(texture.pixelFormat == m_pixelFormat);
    m_targetTexture = texture;
}
} // namespace rive::pls
