/*
 * Copyright 2022 Rive
 */

#include "rive/pls/pls_render_context_buffer_ring_impl.hpp"

namespace rive::pls
{
void PLSRenderContextBufferRingImpl::resizePathTexture(size_t width, size_t height)
{
    m_pathBuffer = makeTexelBufferRing(TexelBufferRing::Format::rgba32ui,
                                       width / kPathTexelsPerItem,
                                       height,
                                       kPathTexelsPerItem,
                                       kPathTextureIdx,
                                       TexelBufferRing::Filter::nearest);
}

void PLSRenderContextBufferRingImpl::resizeContourTexture(size_t width, size_t height)
{
    m_contourBuffer = makeTexelBufferRing(TexelBufferRing::Format::rgba32ui,
                                          width / kContourTexelsPerItem,
                                          height,
                                          kContourTexelsPerItem,
                                          pls::kContourTextureIdx,
                                          TexelBufferRing::Filter::nearest);
}

void PLSRenderContextBufferRingImpl::resizeSimpleColorRampsBuffer(size_t sizeInBytes)
{
    m_simpleColorRampsBuffer =
        makePixelUnpackBufferRing(sizeInBytes / sizeof(TwoTexelRamp), sizeof(TwoTexelRamp));
}

void PLSRenderContextBufferRingImpl::resizeGradSpanBuffer(size_t sizeInBytes)
{
    m_gradSpanBuffer =
        makeVertexBufferRing(sizeInBytes / sizeof(GradientSpan), sizeof(GradientSpan));
}

void PLSRenderContextBufferRingImpl::resizeTessVertexSpanBuffer(size_t sizeInBytes)
{
    m_tessSpanBuffer =
        makeVertexBufferRing(sizeInBytes / sizeof(TessVertexSpan), sizeof(TessVertexSpan));
}

void PLSRenderContextBufferRingImpl::resizeTriangleVertexBuffer(size_t sizeInBytes)
{
    m_triangleBuffer =
        makeVertexBufferRing(sizeInBytes / sizeof(TriangleVertex), sizeof(TriangleVertex));
}

void PLSRenderContextBufferRingImpl::unmapPathTexture(size_t widthWritten, size_t heightWritten)
{
    m_pathBuffer->unmapAndSubmitBuffer(heightWritten * widthWritten * 4 * 4);
}

void PLSRenderContextBufferRingImpl::unmapContourTexture(size_t widthWritten, size_t heightWritten)
{
    return m_contourBuffer->unmapAndSubmitBuffer(heightWritten * widthWritten * 4 * 4);
}

void PLSRenderContextBufferRingImpl::unmapSimpleColorRampsBuffer(size_t bytesWritten)
{
    m_simpleColorRampsBuffer->unmapAndSubmitBuffer(bytesWritten);
}

void PLSRenderContextBufferRingImpl::unmapGradSpanBuffer(size_t bytesWritten)
{
    m_gradSpanBuffer->unmapAndSubmitBuffer(bytesWritten);
}

void PLSRenderContextBufferRingImpl::unmapTessVertexSpanBuffer(size_t bytesWritten)
{
    m_tessSpanBuffer->unmapAndSubmitBuffer(bytesWritten);
}

void PLSRenderContextBufferRingImpl::unmapTriangleVertexBuffer(size_t bytesWritten)
{
    m_triangleBuffer->unmapAndSubmitBuffer(bytesWritten);
}

void PLSRenderContextBufferRingImpl::updateFlushUniforms(const FlushUniforms* uniformData)
{
    if (m_uniformBuffer == nullptr)
    {
        m_uniformBuffer = makeUniformBufferRing(sizeof(FlushUniforms));
    }
    memcpy(m_uniformBuffer->mapBuffer(), uniformData, sizeof(FlushUniforms));
    m_uniformBuffer->unmapAndSubmitBuffer(sizeof(FlushUniforms));
}
} // namespace rive::pls
