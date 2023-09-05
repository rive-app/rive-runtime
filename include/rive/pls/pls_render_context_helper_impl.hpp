/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls_render_context_impl.hpp"
#include "rive/pls/buffer_ring.hpp"

namespace rive::pls
{
// PLSRenderContextImpl that uses BufferRing to manage GPU resources.
class PLSRenderContextHelperImpl : public PLSRenderContextImpl
{
public:
    rcp<PLSTexture> decodeImageTexture(Span<const uint8_t> encodedBytes) override;

    void resizePathTexture(size_t width, size_t height) override;
    void resizeContourTexture(size_t width, size_t height) override;
    void resizeSimpleColorRampsBuffer(size_t sizeInBytes) override;
    void resizeGradSpanBuffer(size_t sizeInBytes) override;
    void resizeTessVertexSpanBuffer(size_t sizeInBytes) override;
    void resizeTriangleVertexBuffer(size_t sizeInBytes) override;
    void resizeImageMeshUniformBuffer(size_t sizeInBytes) override;

    void mapPathTexture(WriteOnlyMappedMemory<pls::PathData>*) override;
    void mapContourTexture(WriteOnlyMappedMemory<pls::ContourData>*) override;
    void mapSimpleColorRampsBuffer(WriteOnlyMappedMemory<pls::TwoTexelRamp>*) override;
    void mapGradSpanBuffer(WriteOnlyMappedMemory<pls::GradientSpan>*) override;
    void mapTessVertexSpanBuffer(WriteOnlyMappedMemory<pls::TessVertexSpan>*) override;
    void mapTriangleVertexBuffer(WriteOnlyMappedMemory<pls::TriangleVertex>*) override;
    void mapImageMeshUniformBuffer(WriteOnlyMappedMemory<pls::ImageMeshUniforms>*) override;
    void mapFlushUniformBuffer(WriteOnlyMappedMemory<pls::FlushUniforms>*) override;

    void unmapPathTexture(size_t widthWritten, size_t heightWritten) override;
    void unmapContourTexture(size_t widthWritten, size_t heightWritten) override;
    void unmapSimpleColorRampsBuffer(size_t bytesWritten) override;
    void unmapGradSpanBuffer(size_t bytesWritten) override;
    void unmapTessVertexSpanBuffer(size_t bytesWritten) override;
    void unmapTriangleVertexBuffer(size_t bytesWritten) override;
    void unmapImageMeshUniformBuffer(size_t bytesWritten) override;
    void unmapFlushUniformBuffer() override;

protected:
    const TexelBufferRing* pathBufferRing() { return m_pathBuffer.get(); }
    const TexelBufferRing* contourBufferRing() { return m_contourBuffer.get(); }
    const BufferRing* simpleColorRampsBufferRing() const { return m_simpleColorRampsBuffer.get(); }
    const BufferRing* gradSpanBufferRing() const { return m_gradSpanBuffer.get(); }
    const BufferRing* tessSpanBufferRing() { return m_tessSpanBuffer.get(); }
    const BufferRing* triangleBufferRing() { return m_triangleBuffer.get(); }
    const BufferRing* imageMeshUniformBufferRing() const { return m_imageMeshUniformBuffer.get(); }
    const BufferRing* flushUniformBufferRing() const { return m_flushUniformBuffer.get(); }

    virtual rcp<PLSTexture> makeImageTexture(uint32_t width,
                                             uint32_t height,
                                             uint32_t mipLevelCount,
                                             const uint8_t imageDataRGBA[]) = 0;

    virtual std::unique_ptr<TexelBufferRing> makeTexelBufferRing(TexelBufferRing::Format,
                                                                 size_t widthInItems,
                                                                 size_t height,
                                                                 size_t texelsPerItem,
                                                                 int textureIdx,
                                                                 TexelBufferRing::Filter) = 0;

    virtual std::unique_ptr<BufferRing> makeVertexBufferRing(size_t capacity,
                                                             size_t itemSizeInBytes) = 0;

    virtual std::unique_ptr<BufferRing> makePixelUnpackBufferRing(size_t capacity,
                                                                  size_t itemSizeInBytes) = 0;

    virtual std::unique_ptr<BufferRing> makeUniformBufferRing(size_t capacity,
                                                              size_t sizeInBytes) = 0;

private:
    std::unique_ptr<TexelBufferRing> m_pathBuffer;
    std::unique_ptr<TexelBufferRing> m_contourBuffer;
    std::unique_ptr<BufferRing> m_simpleColorRampsBuffer;
    std::unique_ptr<BufferRing> m_gradSpanBuffer;
    std::unique_ptr<BufferRing> m_tessSpanBuffer;
    std::unique_ptr<BufferRing> m_triangleBuffer;
    std::unique_ptr<BufferRing> m_imageMeshUniformBuffer;
    std::unique_ptr<BufferRing> m_flushUniformBuffer;
};
} // namespace rive::pls
