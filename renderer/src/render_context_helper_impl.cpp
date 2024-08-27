/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/render_context_helper_impl.hpp"

#include "rive/renderer/image.hpp"
#include "shaders/constants.glsl"

#ifdef RIVE_DECODERS
#include "rive/decoders/bitmap_decoder.hpp"
#endif

namespace rive::gpu
{
rcp<PLSTexture> PLSRenderContextHelperImpl::decodeImageTexture(Span<const uint8_t> encodedBytes)
{
#ifdef RIVE_DECODERS
    auto bitmap = Bitmap::decode(encodedBytes.data(), encodedBytes.size());
    if (bitmap)
    {
        // For now, PLSRenderContextImpl::makeImageTexture() only accepts RGBA.
        if (bitmap->pixelFormat() != Bitmap::PixelFormat::RGBA)
        {
            bitmap->pixelFormat(Bitmap::PixelFormat::RGBA);
        }
        uint32_t width = bitmap->width();
        uint32_t height = bitmap->height();
        uint32_t mipLevelCount = math::msb(height | width);
        return makeImageTexture(width, height, mipLevelCount, bitmap->bytes());
    }
#endif
    return nullptr;
}

void PLSRenderContextHelperImpl::resizeFlushUniformBuffer(size_t sizeInBytes)
{
    m_flushUniformBuffer = makeUniformBufferRing(sizeInBytes);
}

void PLSRenderContextHelperImpl::resizeImageDrawUniformBuffer(size_t sizeInBytes)
{
    m_imageDrawUniformBuffer = makeUniformBufferRing(sizeInBytes);
}

void PLSRenderContextHelperImpl::resizePathBuffer(size_t sizeInBytes,
                                                  gpu::StorageBufferStructure bufferStructure)
{
    m_pathBuffer = makeStorageBufferRing(sizeInBytes, bufferStructure);
}

void PLSRenderContextHelperImpl::resizePaintBuffer(size_t sizeInBytes,
                                                   gpu::StorageBufferStructure bufferStructure)
{
    m_paintBuffer = makeStorageBufferRing(sizeInBytes, bufferStructure);
}

void PLSRenderContextHelperImpl::resizePaintAuxBuffer(size_t sizeInBytes,
                                                      gpu::StorageBufferStructure bufferStructure)
{
    m_paintAuxBuffer = makeStorageBufferRing(sizeInBytes, bufferStructure);
}

void PLSRenderContextHelperImpl::resizeContourBuffer(size_t sizeInBytes,
                                                     gpu::StorageBufferStructure bufferStructure)
{
    m_contourBuffer = makeStorageBufferRing(sizeInBytes, bufferStructure);
}

void PLSRenderContextHelperImpl::resizeSimpleColorRampsBuffer(size_t sizeInBytes)
{
    m_simpleColorRampsBuffer = makeTextureTransferBufferRing(sizeInBytes);
}

void PLSRenderContextHelperImpl::resizeGradSpanBuffer(size_t sizeInBytes)
{
    m_gradSpanBuffer = makeVertexBufferRing(sizeInBytes);
}

void PLSRenderContextHelperImpl::resizeTessVertexSpanBuffer(size_t sizeInBytes)
{
    m_tessSpanBuffer = makeVertexBufferRing(sizeInBytes);
}

void PLSRenderContextHelperImpl::resizeTriangleVertexBuffer(size_t sizeInBytes)
{
    m_triangleBuffer = makeVertexBufferRing(sizeInBytes);
}

void* PLSRenderContextHelperImpl::mapFlushUniformBuffer(size_t mapSizeInBytes)
{
    return m_flushUniformBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapImageDrawUniformBuffer(size_t mapSizeInBytes)
{
    return m_imageDrawUniformBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapPathBuffer(size_t mapSizeInBytes)
{
    return m_pathBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapPaintBuffer(size_t mapSizeInBytes)
{
    return m_paintBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapPaintAuxBuffer(size_t mapSizeInBytes)
{
    return m_paintAuxBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapContourBuffer(size_t mapSizeInBytes)
{
    return m_contourBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapSimpleColorRampsBuffer(size_t mapSizeInBytes)
{
    return m_simpleColorRampsBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapGradSpanBuffer(size_t mapSizeInBytes)
{
    return m_gradSpanBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapTessVertexSpanBuffer(size_t mapSizeInBytes)
{
    return m_tessSpanBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapTriangleVertexBuffer(size_t mapSizeInBytes)
{
    return m_triangleBuffer->mapBuffer(mapSizeInBytes);
}

void PLSRenderContextHelperImpl::unmapFlushUniformBuffer()
{
    m_flushUniformBuffer->unmapAndSubmitBuffer();
}

void PLSRenderContextHelperImpl::unmapImageDrawUniformBuffer()
{
    m_imageDrawUniformBuffer->unmapAndSubmitBuffer();
}

void PLSRenderContextHelperImpl::unmapPathBuffer() { m_pathBuffer->unmapAndSubmitBuffer(); }

void PLSRenderContextHelperImpl::unmapPaintBuffer() { m_paintBuffer->unmapAndSubmitBuffer(); }

void PLSRenderContextHelperImpl::unmapPaintAuxBuffer() { m_paintAuxBuffer->unmapAndSubmitBuffer(); }

void PLSRenderContextHelperImpl::unmapContourBuffer() { m_contourBuffer->unmapAndSubmitBuffer(); }

void PLSRenderContextHelperImpl::unmapSimpleColorRampsBuffer()
{
    m_simpleColorRampsBuffer->unmapAndSubmitBuffer();
}

void PLSRenderContextHelperImpl::unmapGradSpanBuffer() { m_gradSpanBuffer->unmapAndSubmitBuffer(); }

void PLSRenderContextHelperImpl::unmapTessVertexSpanBuffer()
{
    m_tessSpanBuffer->unmapAndSubmitBuffer();
}

void PLSRenderContextHelperImpl::unmapTriangleVertexBuffer()
{
    m_triangleBuffer->unmapAndSubmitBuffer();
}
} // namespace rive::gpu
