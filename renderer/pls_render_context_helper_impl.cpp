/*
 * Copyright 2022 Rive
 */

#include "rive/pls/pls_render_context_helper_impl.hpp"

#include "rive/pls/pls_image.hpp"
#include "shaders/constants.glsl"

#ifdef RIVE_DECODERS
#include "rive/decoders/bitmap_decoder.hpp"
#endif

namespace rive::pls
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

void PLSRenderContextHelperImpl::resizePathTexture(size_t width, size_t height)
{
    m_pathBuffer = makeTexelBufferRing(TexelBufferRing::Format::rgba32ui,
                                       width / kPathTexelsPerItem,
                                       height,
                                       kPathTexelsPerItem,
                                       PATH_TEXTURE_IDX,
                                       TexelBufferRing::Filter::nearest);
}

void PLSRenderContextHelperImpl::resizeContourTexture(size_t width, size_t height)
{
    m_contourBuffer = makeTexelBufferRing(TexelBufferRing::Format::rgba32ui,
                                          width / kContourTexelsPerItem,
                                          height,
                                          kContourTexelsPerItem,
                                          CONTOUR_TEXTURE_IDX,
                                          TexelBufferRing::Filter::nearest);
}

void PLSRenderContextHelperImpl::resizeSimpleColorRampsBuffer(size_t sizeInBytes)
{
    m_simpleColorRampsBuffer =
        makePixelUnpackBufferRing(sizeInBytes / sizeof(TwoTexelRamp), sizeof(TwoTexelRamp));
}

void PLSRenderContextHelperImpl::resizeGradSpanBuffer(size_t sizeInBytes)
{
    m_gradSpanBuffer =
        makeVertexBufferRing(sizeInBytes / sizeof(GradientSpan), sizeof(GradientSpan));
}

void PLSRenderContextHelperImpl::resizeTessVertexSpanBuffer(size_t sizeInBytes)
{
    m_tessSpanBuffer =
        makeVertexBufferRing(sizeInBytes / sizeof(TessVertexSpan), sizeof(TessVertexSpan));
}

void PLSRenderContextHelperImpl::resizeTriangleVertexBuffer(size_t sizeInBytes)
{
    m_triangleBuffer =
        makeVertexBufferRing(sizeInBytes / sizeof(TriangleVertex), sizeof(TriangleVertex));
}

void PLSRenderContextHelperImpl::resizeImageDrawUniformBuffer(size_t sizeInBytes)
{
    m_imageDrawUniformBuffer = makeUniformBufferRing(sizeInBytes / sizeof(pls::ImageDrawUniforms),
                                                     sizeof(pls::ImageDrawUniforms));
}

void PLSRenderContextHelperImpl::mapPathTexture(WriteOnlyMappedMemory<PathData>* pathData)
{
    pathData->reset(m_pathBuffer->mapBuffer(), m_pathBuffer->capacity());
}

void PLSRenderContextHelperImpl::mapContourTexture(WriteOnlyMappedMemory<ContourData>* contourData)
{
    contourData->reset(m_contourBuffer->mapBuffer(), m_contourBuffer->capacity());
}

void PLSRenderContextHelperImpl::mapSimpleColorRampsBuffer(
    WriteOnlyMappedMemory<TwoTexelRamp>* simpleColorRampsData)
{
    simpleColorRampsData->reset(m_simpleColorRampsBuffer->mapBuffer(),
                                m_simpleColorRampsBuffer->capacity());
}

void PLSRenderContextHelperImpl::mapGradSpanBuffer(
    WriteOnlyMappedMemory<GradientSpan>* gradSpanData)
{
    gradSpanData->reset(m_gradSpanBuffer->mapBuffer(), m_gradSpanBuffer->capacity());
}

void PLSRenderContextHelperImpl::mapTessVertexSpanBuffer(
    WriteOnlyMappedMemory<TessVertexSpan>* tessVertexSpanData)
{
    tessVertexSpanData->reset(m_tessSpanBuffer->mapBuffer(), m_tessSpanBuffer->capacity());
}

void PLSRenderContextHelperImpl::mapTriangleVertexBuffer(
    WriteOnlyMappedMemory<TriangleVertex>* triangleVertexData)
{
    triangleVertexData->reset(m_triangleBuffer->mapBuffer(), m_triangleBuffer->capacity());
}

void PLSRenderContextHelperImpl::mapImageDrawUniformBuffer(
    WriteOnlyMappedMemory<pls::ImageDrawUniforms>* imageDrawUniformData)
{
    imageDrawUniformData->reset(m_imageDrawUniformBuffer->mapBuffer(),
                                m_imageDrawUniformBuffer->capacity());
}

void PLSRenderContextHelperImpl::mapFlushUniformBuffer(
    WriteOnlyMappedMemory<pls::FlushUniforms>* flushUniformData)
{
    if (m_flushUniformBuffer == nullptr)
    {
        // Allocate the flushUniformBuffer lazily size it doesn't have a corresponding 'resize()'
        // call where we can allocate it.
        m_flushUniformBuffer = makeUniformBufferRing(1, sizeof(pls::FlushUniforms));
    }
    flushUniformData->reset(m_flushUniformBuffer->mapBuffer(), 1);
}

void PLSRenderContextHelperImpl::unmapPathTexture(size_t widthWritten, size_t heightWritten)
{
    m_pathBuffer->unmapAndSubmitBuffer(heightWritten * widthWritten * 4 * 4);
}

void PLSRenderContextHelperImpl::unmapContourTexture(size_t widthWritten, size_t heightWritten)
{
    m_contourBuffer->unmapAndSubmitBuffer(heightWritten * widthWritten * 4 * 4);
}

void PLSRenderContextHelperImpl::unmapSimpleColorRampsBuffer(size_t bytesWritten)
{
    m_simpleColorRampsBuffer->unmapAndSubmitBuffer(bytesWritten);
}

void PLSRenderContextHelperImpl::unmapGradSpanBuffer(size_t bytesWritten)
{
    m_gradSpanBuffer->unmapAndSubmitBuffer(bytesWritten);
}

void PLSRenderContextHelperImpl::unmapTessVertexSpanBuffer(size_t bytesWritten)
{
    m_tessSpanBuffer->unmapAndSubmitBuffer(bytesWritten);
}

void PLSRenderContextHelperImpl::unmapTriangleVertexBuffer(size_t bytesWritten)
{
    m_triangleBuffer->unmapAndSubmitBuffer(bytesWritten);
}

void PLSRenderContextHelperImpl::unmapImageDrawUniformBuffer(size_t bytesWritten)
{
    m_imageDrawUniformBuffer->unmapAndSubmitBuffer(bytesWritten);
}

void PLSRenderContextHelperImpl::unmapFlushUniformBuffer()
{
    m_flushUniformBuffer->unmapAndSubmitBuffer(sizeof(pls::FlushUniforms));
}
} // namespace rive::pls
