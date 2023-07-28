/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls_render_context_impl.hpp"
#include "rive/pls/buffer_ring.hpp"

namespace rive::pls
{
class PLSRenderContextBufferRingImpl : public PLSRenderContextImpl
{
public:
    PLSRenderContextBufferRingImpl(const PlatformFeatures& platformFeatures)
    {
        m_platformFeatures = platformFeatures;
    }

    void resizePathTexture(size_t width, size_t height) override;
    void resizeContourTexture(size_t width, size_t height) override;
    void resizeSimpleColorRampsBuffer(size_t sizeInBytes) override;
    void resizeGradSpanBuffer(size_t sizeInBytes) override;
    void resizeTessVertexSpanBuffer(size_t sizeInBytes) override;
    void resizeTriangleVertexBuffer(size_t sizeInBytes) override;

    void resizeGradientTexture(size_t height) override { allocateGradientTexture(height); }
    void resizeTessellationTexture(size_t height) override { allocateTessellationTexture(height); }

    void* mapPathTexture() override { return m_pathBuffer->mapBuffer(); }
    void* mapContourTexture() override { return m_contourBuffer->mapBuffer(); }
    void* mapSimpleColorRampsBuffer() override { return m_simpleColorRampsBuffer->mapBuffer(); }
    void* mapGradSpanBuffer() override { return m_gradSpanBuffer->mapBuffer(); }
    void* mapTessVertexSpanBuffer() override { return m_tessSpanBuffer->mapBuffer(); }
    void* mapTriangleVertexBuffer() override { return m_triangleBuffer->mapBuffer(); }

    void unmapPathTexture(size_t widthWritten, size_t heightWritten) override;
    void unmapContourTexture(size_t widthWritten, size_t heightWritten) override;
    void unmapSimpleColorRampsBuffer(size_t bytesWritten) override;
    void unmapGradSpanBuffer(size_t bytesWritten) override;
    void unmapTessVertexSpanBuffer(size_t bytesWritten) override;
    void unmapTriangleVertexBuffer(size_t bytesWritten) override;

    void updateFlushUniforms(const FlushUniforms*) override;

protected:
    const TexelBufferRing* pathBufferRing() { return m_pathBuffer.get(); }
    const TexelBufferRing* contourBufferRing() { return m_contourBuffer.get(); }
    const BufferRingImpl* simpleColorRampsBufferRing() const
    {
        return m_simpleColorRampsBuffer.get();
    }
    const BufferRingImpl* gradSpanBufferRing() const { return m_gradSpanBuffer.get(); }
    const BufferRingImpl* tessSpanBufferRing() { return m_tessSpanBuffer.get(); }
    const BufferRingImpl* triangleBufferRing() { return m_triangleBuffer.get(); }
    const BufferRingImpl* uniformBufferRing() const { return m_uniformBuffer.get(); }

    virtual std::unique_ptr<BufferRingImpl> makeVertexBufferRing(size_t capacity,
                                                                 size_t itemSizeInBytes) = 0;

    virtual std::unique_ptr<TexelBufferRing> makeTexelBufferRing(TexelBufferRing::Format,
                                                                 size_t widthInItems,
                                                                 size_t height,
                                                                 size_t texelsPerItem,
                                                                 int textureIdx,
                                                                 TexelBufferRing::Filter) = 0;

    virtual std::unique_ptr<BufferRingImpl> makePixelUnpackBufferRing(size_t capacity,
                                                                      size_t itemSizeInBytes) = 0;

    virtual std::unique_ptr<BufferRingImpl> makeUniformBufferRing(size_t sizeInBytes) = 0;

    virtual void allocateGradientTexture(size_t height) = 0;
    virtual void allocateTessellationTexture(size_t height) = 0;

private:
    std::unique_ptr<TexelBufferRing> m_pathBuffer;
    std::unique_ptr<TexelBufferRing> m_contourBuffer;
    std::unique_ptr<BufferRingImpl> m_simpleColorRampsBuffer;
    std::unique_ptr<BufferRingImpl> m_gradSpanBuffer;
    std::unique_ptr<BufferRingImpl> m_tessSpanBuffer;
    std::unique_ptr<BufferRingImpl> m_triangleBuffer;
    std::unique_ptr<BufferRingImpl> m_uniformBuffer;
};
} // namespace rive::pls
