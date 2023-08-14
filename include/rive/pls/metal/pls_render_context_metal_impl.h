/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls_render_context_buffer_ring_impl.hpp"
#include <map>
#include <mutex>

#ifndef RIVE_OBJC_NOP
#import <Metal/Metal.h>
#endif

namespace rive::pls
{
// Metal backend implementation of PLSRenderTarget.
class PLSRenderTargetMetal : public PLSRenderTarget
{
public:
    ~PLSRenderTargetMetal() override {}

    MTLPixelFormat pixelFormat() const { return m_pixelFormat; }

    void setTargetTexture(id<MTLTexture> texture);
    id<MTLTexture> targetTexture() const { return m_targetTexture; }

private:
    friend class PLSRenderContextMetalImpl;

    PLSRenderTargetMetal(id<MTLDevice> gpu,
                         MTLPixelFormat pixelFormat,
                         size_t width,
                         size_t height,
                         const PlatformFeatures&);

    const MTLPixelFormat m_pixelFormat;
    id<MTLTexture> m_targetTexture;
    id<MTLTexture> m_coverageMemorylessTexture;
    id<MTLTexture> m_originalDstColorMemorylessTexture;
    id<MTLTexture> m_clipMemorylessTexture;
};

// Metal backend implementation of PLSRenderContextImpl.
class PLSRenderContextMetalImpl : public PLSRenderContextBufferRingImpl
{
public:
    static std::unique_ptr<PLSRenderContext> MakeContext(id<MTLDevice>, id<MTLCommandQueue>);
    ~PLSRenderContextMetalImpl() override;

    id<MTLDevice> gpu() const { return m_gpu; }

    rcp<PLSRenderTargetMetal> makeRenderTarget(MTLPixelFormat, size_t width, size_t height);

private:
    PLSRenderContextMetalImpl(id<MTLDevice>, id<MTLCommandQueue>);

    rcp<PLSTexture> makeImageTexture(uint32_t width,
                                     uint32_t height,
                                     uint32_t mipLevelCount,
                                     const uint8_t imageDataRGBA[]) override;

    std::unique_ptr<TexelBufferRing> makeTexelBufferRing(TexelBufferRing::Format,
                                                         size_t widthInItems,
                                                         size_t height,
                                                         size_t texelsPerItem,
                                                         int textureIdx,
                                                         TexelBufferRing::Filter) override;

    std::unique_ptr<BufferRing> makeVertexBufferRing(size_t capacity,
                                                     size_t itemSizeInBytes) override;

    std::unique_ptr<BufferRing> makePixelUnpackBufferRing(size_t capacity,
                                                          size_t itemSizeInBytes) override;
    std::unique_ptr<BufferRing> makeUniformBufferRing(size_t itemSizeInBytes) override;

    void resizeGradientTexture(size_t height) override;
    void resizeTessellationTexture(size_t height) override;

    // Obtains an exclusive lock on the next buffer ring index, potentially blocking until the GPU
    // has finished rendering with it. This ensures it is safe for the CPU to begin modifying the
    // next buffers in our rings.
    void prepareToMapBuffers() override;

    void flush(const PLSRenderContext::FlushDescriptor&) override;

    const id<MTLDevice> m_gpu;
    const id<MTLCommandQueue> m_queue;

    id<MTLLibrary> m_plsLibrary;

    // Renders color ramps to the gradient texture.
    class ColorRampPipeline;
    std::unique_ptr<ColorRampPipeline> m_colorRampPipeline;
    id<MTLTexture> m_gradientTexture = nullptr;

    // Renders tessellated vertices to the tessellation texture.
    class TessellatePipeline;
    std::unique_ptr<TessellatePipeline> m_tessPipeline;
    id<MTLTexture> m_tessVertexTexture = nullptr;

    // Renders paths to the main render target.
    class DrawPipeline;
    std::map<uint32_t, DrawPipeline> m_drawPipelines;
    id<MTLBuffer> m_pathPatchVertexBuffer;
    id<MTLBuffer> m_pathPatchIndexBuffer;

    // Locks buffer contents until the GPU has finished rendering with them. Prevents the CPU from
    // overriding data before the GPU is done with it.
    std::mutex m_bufferRingLocks[kBufferRingSize];
    int m_bufferRingIdx = 0;
};
} // namespace rive::pls
