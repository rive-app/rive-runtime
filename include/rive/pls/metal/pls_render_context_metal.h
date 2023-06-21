/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls_render_context.hpp"
#include <map>
#include <mutex>

#ifndef RIVE_OBJC_NOP
#import <Metal/Metal.h>
#endif

namespace rive::pls
{
class PLSRenderTargetMetal;

// Metal backend implementation of PLSRenderContext.
class PLSRenderContextMetal : public PLSRenderContext
{
public:
    static std::unique_ptr<PLSRenderContextMetal> Make(id<MTLDevice>, id<MTLCommandQueue>);
    ~PLSRenderContextMetal() override;

    id<MTLDevice> gpu() const { return m_gpu; }

    rcp<PLSRenderTargetMetal> makeRenderTarget(MTLPixelFormat, size_t width, size_t height);

private:
    PLSRenderContextMetal(const PlatformFeatures&, id<MTLDevice>, id<MTLCommandQueue>);

    std::unique_ptr<BufferRingImpl> makeVertexBufferRing(size_t capacity,
                                                         size_t itemSizeInBytes) override;

    std::unique_ptr<TexelBufferRing> makeTexelBufferRing(TexelBufferRing::Format,
                                                         size_t widthInItems,
                                                         size_t height,
                                                         size_t texelsPerItem,
                                                         int textureIdx,
                                                         TexelBufferRing::Filter) override;

    std::unique_ptr<BufferRingImpl> makeUniformBufferRing(size_t capacity,
                                                          size_t itemSizeInBytes) override;

    void allocateTessellationTexture(size_t height) override;

    void onBeginFrame() override { lockNextBufferRingIndex(); }

    // Obtains an exclusive lock on the next buffer ring index, potentially blocking until the GPU
    // has finished rendering with it. This ensures it is safe for the CPU to begin modifying the
    // next buffers in our rings.
    void lockNextBufferRingIndex();

    void onFlush(FlushType,
                 LoadAction,
                 size_t gradSpanCount,
                 size_t gradSpansHeight,
                 size_t tessVertexSpanCount,
                 size_t tessDataHeight,
                 bool needsClipBuffer) override;

    const id<MTLDevice> m_gpu;
    const id<MTLCommandQueue> m_queue;

    id<MTLLibrary> m_plsLibrary;

    // Renders color ramps to the gradient texture.
    class ColorRampPipeline;
    std::unique_ptr<ColorRampPipeline> m_colorRampPipeline;

    // Renders tessellated vertices to the tessellation texture.
    class TessellatePipeline;
    std::unique_ptr<TessellatePipeline> m_tessPipeline;
    id<MTLTexture> m_tessVertexTexture = nullptr;

    // Renders paths to the main render target.
    class DrawPipeline;
    std::map<uint64_t, DrawPipeline> m_drawPipelines;
    id<MTLBuffer> m_pathWedgeVertexBuffer;
    id<MTLBuffer> m_pathWedgeIndexBuffer;

    // Locks buffer contents until the GPU has finished rendering with them. Prevents the CPU from
    // overriding data before the GPU is done with it.
    std::mutex m_bufferRingLocks[kBufferRingSize];
    int m_bufferRingIdx = 0;
};
} // namespace rive::pls
