/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/render_context_helper_impl.hpp"

#include "rive/renderer/rive_render_image.hpp"
#include "shaders/constants.glsl"

#ifdef RIVE_DECODERS
#include "rive/decoders/bitmap_decoder.hpp"
#endif

namespace rive::gpu
{
rcp<Texture> RenderContextHelperImpl::decodeImageTexture(Span<const uint8_t> encodedBytes)
{
#ifdef RIVE_DECODERS
    auto bitmap = Bitmap::decode(encodedBytes.data(), encodedBytes.size());
    if (bitmap)
    {
        // For now, RenderContextImpl::makeImageTexture() only accepts RGBA.
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

void RenderContextHelperImpl::resizeFlushUniformBuffer(size_t sizeInBytes)
{
    m_flushUniformBuffer = makeUniformBufferRing(sizeInBytes);
}

void RenderContextHelperImpl::resizeImageDrawUniformBuffer(size_t sizeInBytes)
{
    m_imageDrawUniformBuffer = makeUniformBufferRing(sizeInBytes);
}

void RenderContextHelperImpl::resizePathBuffer(size_t sizeInBytes,
                                               gpu::StorageBufferStructure bufferStructure)
{
    m_pathBuffer = makeStorageBufferRing(sizeInBytes, bufferStructure);
}

void RenderContextHelperImpl::resizePaintBuffer(size_t sizeInBytes,
                                                gpu::StorageBufferStructure bufferStructure)
{
    m_paintBuffer = makeStorageBufferRing(sizeInBytes, bufferStructure);
}

void RenderContextHelperImpl::resizePaintAuxBuffer(size_t sizeInBytes,
                                                   gpu::StorageBufferStructure bufferStructure)
{
    m_paintAuxBuffer = makeStorageBufferRing(sizeInBytes, bufferStructure);
}

void RenderContextHelperImpl::resizeContourBuffer(size_t sizeInBytes,
                                                  gpu::StorageBufferStructure bufferStructure)
{
    m_contourBuffer = makeStorageBufferRing(sizeInBytes, bufferStructure);
}

void RenderContextHelperImpl::resizeSimpleColorRampsBuffer(size_t sizeInBytes)
{
    m_simpleColorRampsBuffer = makeTextureTransferBufferRing(sizeInBytes);
}

void RenderContextHelperImpl::resizeGradSpanBuffer(size_t sizeInBytes)
{
    m_gradSpanBuffer = makeVertexBufferRing(sizeInBytes);
}

void RenderContextHelperImpl::resizeTessVertexSpanBuffer(size_t sizeInBytes)
{
    m_tessSpanBuffer = makeVertexBufferRing(sizeInBytes);
}

void RenderContextHelperImpl::resizeTriangleVertexBuffer(size_t sizeInBytes)
{
    m_triangleBuffer = makeVertexBufferRing(sizeInBytes);
}

void* RenderContextHelperImpl::mapFlushUniformBuffer(size_t mapSizeInBytes)
{
    return m_flushUniformBuffer->mapBuffer(mapSizeInBytes);
}

void* RenderContextHelperImpl::mapImageDrawUniformBuffer(size_t mapSizeInBytes)
{
    return m_imageDrawUniformBuffer->mapBuffer(mapSizeInBytes);
}

void* RenderContextHelperImpl::mapPathBuffer(size_t mapSizeInBytes)
{
    return m_pathBuffer->mapBuffer(mapSizeInBytes);
}

void* RenderContextHelperImpl::mapPaintBuffer(size_t mapSizeInBytes)
{
    return m_paintBuffer->mapBuffer(mapSizeInBytes);
}

void* RenderContextHelperImpl::mapPaintAuxBuffer(size_t mapSizeInBytes)
{
    return m_paintAuxBuffer->mapBuffer(mapSizeInBytes);
}

void* RenderContextHelperImpl::mapContourBuffer(size_t mapSizeInBytes)
{
    return m_contourBuffer->mapBuffer(mapSizeInBytes);
}

void* RenderContextHelperImpl::mapSimpleColorRampsBuffer(size_t mapSizeInBytes)
{
    return m_simpleColorRampsBuffer->mapBuffer(mapSizeInBytes);
}

void* RenderContextHelperImpl::mapGradSpanBuffer(size_t mapSizeInBytes)
{
    return m_gradSpanBuffer->mapBuffer(mapSizeInBytes);
}

void* RenderContextHelperImpl::mapTessVertexSpanBuffer(size_t mapSizeInBytes)
{
    return m_tessSpanBuffer->mapBuffer(mapSizeInBytes);
}

void* RenderContextHelperImpl::mapTriangleVertexBuffer(size_t mapSizeInBytes)
{
    return m_triangleBuffer->mapBuffer(mapSizeInBytes);
}

void RenderContextHelperImpl::unmapFlushUniformBuffer()
{
    m_flushUniformBuffer->unmapAndSubmitBuffer();
}

void RenderContextHelperImpl::unmapImageDrawUniformBuffer()
{
    m_imageDrawUniformBuffer->unmapAndSubmitBuffer();
}

void RenderContextHelperImpl::unmapPathBuffer() { m_pathBuffer->unmapAndSubmitBuffer(); }

void RenderContextHelperImpl::unmapPaintBuffer() { m_paintBuffer->unmapAndSubmitBuffer(); }

void RenderContextHelperImpl::unmapPaintAuxBuffer() { m_paintAuxBuffer->unmapAndSubmitBuffer(); }

void RenderContextHelperImpl::unmapContourBuffer() { m_contourBuffer->unmapAndSubmitBuffer(); }

void RenderContextHelperImpl::unmapSimpleColorRampsBuffer()
{
    m_simpleColorRampsBuffer->unmapAndSubmitBuffer();
}

void RenderContextHelperImpl::unmapGradSpanBuffer() { m_gradSpanBuffer->unmapAndSubmitBuffer(); }

void RenderContextHelperImpl::unmapTessVertexSpanBuffer()
{
    m_tessSpanBuffer->unmapAndSubmitBuffer();
}

void RenderContextHelperImpl::unmapTriangleVertexBuffer()
{
    m_triangleBuffer->unmapAndSubmitBuffer();
}
} // namespace rive::gpu
