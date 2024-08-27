/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/render_context_impl.hpp"
#include "rive/renderer/buffer_ring.hpp"
#include <chrono>

namespace rive::gpu
{
// PLSRenderContextImpl that uses BufferRing to manage GPU resources.
class PLSRenderContextHelperImpl : public PLSRenderContextImpl
{
public:
    rcp<PLSTexture> decodeImageTexture(Span<const uint8_t> encodedBytes) override;

    void resizeFlushUniformBuffer(size_t sizeInBytes) override;
    void resizeImageDrawUniformBuffer(size_t sizeInBytes) override;
    void resizePathBuffer(size_t sizeInBytes, gpu::StorageBufferStructure) override;
    void resizePaintBuffer(size_t sizeInBytes, gpu::StorageBufferStructure) override;
    void resizePaintAuxBuffer(size_t sizeInBytes, gpu::StorageBufferStructure) override;
    void resizeContourBuffer(size_t sizeInBytes, gpu::StorageBufferStructure) override;
    void resizeSimpleColorRampsBuffer(size_t sizeInBytes) override;
    void resizeGradSpanBuffer(size_t sizeInBytes) override;
    void resizeTessVertexSpanBuffer(size_t sizeInBytes) override;
    void resizeTriangleVertexBuffer(size_t sizeInBytes) override;

    void* mapFlushUniformBuffer(size_t mapSizeInBytes) override;
    void* mapImageDrawUniformBuffer(size_t mapSizeInBytes) override;
    void* mapPathBuffer(size_t mapSizeInBytes) override;
    void* mapPaintBuffer(size_t mapSizeInBytes) override;
    void* mapPaintAuxBuffer(size_t mapSizeInBytes) override;
    void* mapContourBuffer(size_t mapSizeInBytes) override;
    void* mapSimpleColorRampsBuffer(size_t mapSizeInBytes) override;
    void* mapGradSpanBuffer(size_t mapSizeInBytes) override;
    void* mapTessVertexSpanBuffer(size_t mapSizeInBytes) override;
    void* mapTriangleVertexBuffer(size_t mapSizeInBytes) override;

    void unmapFlushUniformBuffer() override;
    void unmapImageDrawUniformBuffer() override;
    void unmapPathBuffer() override;
    void unmapPaintBuffer() override;
    void unmapPaintAuxBuffer() override;
    void unmapContourBuffer() override;
    void unmapSimpleColorRampsBuffer() override;
    void unmapGradSpanBuffer() override;
    void unmapTessVertexSpanBuffer() override;
    void unmapTriangleVertexBuffer() override;

    double secondsNow() const override
    {
        auto elapsed = std::chrono::steady_clock::now() - m_localEpoch;
        return std::chrono::duration<double>(elapsed).count();
    }

protected:
    const BufferRing* flushUniformBufferRing() const { return m_flushUniformBuffer.get(); }
    const BufferRing* imageDrawUniformBufferRing() const { return m_imageDrawUniformBuffer.get(); }
    const BufferRing* pathBufferRing() { return m_pathBuffer.get(); }
    const BufferRing* paintBufferRing() { return m_paintBuffer.get(); }
    const BufferRing* paintAuxBufferRing() { return m_paintAuxBuffer.get(); }
    const BufferRing* contourBufferRing() { return m_contourBuffer.get(); }
    const BufferRing* simpleColorRampsBufferRing() const { return m_simpleColorRampsBuffer.get(); }
    const BufferRing* gradSpanBufferRing() const { return m_gradSpanBuffer.get(); }
    const BufferRing* tessSpanBufferRing() { return m_tessSpanBuffer.get(); }
    const BufferRing* triangleBufferRing() { return m_triangleBuffer.get(); }

    virtual rcp<PLSTexture> makeImageTexture(uint32_t width,
                                             uint32_t height,
                                             uint32_t mipLevelCount,
                                             const uint8_t imageDataRGBA[]) = 0;

    virtual std::unique_ptr<BufferRing> makeUniformBufferRing(size_t capacityInBytes) = 0;
    virtual std::unique_ptr<BufferRing> makeStorageBufferRing(size_t capacityInBytes,
                                                              gpu::StorageBufferStructure) = 0;
    virtual std::unique_ptr<BufferRing> makeVertexBufferRing(size_t capacityInBytes) = 0;
    virtual std::unique_ptr<BufferRing> makeTextureTransferBufferRing(size_t capacityInBytes) = 0;

private:
    std::unique_ptr<BufferRing> m_flushUniformBuffer;
    std::unique_ptr<BufferRing> m_imageDrawUniformBuffer;
    std::unique_ptr<BufferRing> m_pathBuffer;
    std::unique_ptr<BufferRing> m_paintBuffer;
    std::unique_ptr<BufferRing> m_paintAuxBuffer;
    std::unique_ptr<BufferRing> m_contourBuffer;
    std::unique_ptr<BufferRing> m_simpleColorRampsBuffer;
    std::unique_ptr<BufferRing> m_gradSpanBuffer;
    std::unique_ptr<BufferRing> m_tessSpanBuffer;
    std::unique_ptr<BufferRing> m_triangleBuffer;
    std::chrono::steady_clock::time_point m_localEpoch = std::chrono::steady_clock::now();
};
} // namespace rive::gpu
