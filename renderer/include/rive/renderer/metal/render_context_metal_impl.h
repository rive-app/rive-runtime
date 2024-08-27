/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/render_context_helper_impl.hpp"
#include <map>
#include <mutex>

#ifndef RIVE_OBJC_NOP
#import <Metal/Metal.h>
#endif

namespace rive::gpu
{
class BackgroundShaderCompiler;

// Metal backend implementation of PLSRenderTarget.
class PLSRenderTargetMetal : public PLSRenderTarget
{
public:
    ~PLSRenderTargetMetal() override {}

    MTLPixelFormat pixelFormat() const { return m_pixelFormat; }

    bool compatibleWith(id<MTLTexture> texture) const
    {
        assert(texture.usage & MTLTextureUsageRenderTarget);
        return width() == texture.width && height() == texture.height &&
               m_pixelFormat == texture.pixelFormat;
    }

    void setTargetTexture(id<MTLTexture> texture);
    id<MTLTexture> targetTexture() const { return m_targetTexture; }

private:
    friend class PLSRenderContextMetalImpl;

    PLSRenderTargetMetal(
        id<MTLDevice>, MTLPixelFormat, uint32_t width, uint32_t height, const PlatformFeatures&);

    // Lazily-allocated buffers for atomic mode. Unlike the memoryless textures, these buffers have
    // actual physical storage that gets allocated the first time they're accessed.
    id<MTLBuffer> colorAtomicBuffer()
    {
        return m_colorAtomicBuffer != nil ? m_colorAtomicBuffer
                                          : m_colorAtomicBuffer = makeAtomicBuffer();
    }
    id<MTLBuffer> coverageAtomicBuffer()
    {
        return m_coverageAtomicBuffer != nil ? m_coverageAtomicBuffer
                                             : m_coverageAtomicBuffer = makeAtomicBuffer();
    }
    id<MTLBuffer> clipAtomicBuffer()
    {
        return m_clipAtomicBuffer != nil ? m_clipAtomicBuffer
                                         : m_clipAtomicBuffer = makeAtomicBuffer();
    }
    id<MTLBuffer> makeAtomicBuffer()
    {
        return [m_gpu newBufferWithLength:height() * width() * sizeof(uint32_t)
                                  options:MTLResourceStorageModePrivate];
    }

    const id<MTLDevice> m_gpu;
    const MTLPixelFormat m_pixelFormat;

    id<MTLTexture> m_targetTexture = nil;

    id<MTLTexture> m_coverageMemorylessTexture = nil;
    id<MTLTexture> m_clipMemorylessTexture = nil;
    id<MTLTexture> m_scratchColorMemorylessTexture = nil;

    id<MTLBuffer> m_colorAtomicBuffer = nil;
    id<MTLBuffer> m_coverageAtomicBuffer = nil;
    id<MTLBuffer> m_clipAtomicBuffer = nil;
};

// Metal backend implementation of PLSRenderContextImpl.
class PLSRenderContextMetalImpl : public PLSRenderContextHelperImpl
{
public:
    struct ContextOptions
    {
        // Wait for shaders to compile inline with rendering (causing jank), instead of compiling
        // asynchronously in a background thread. (Primarily for testing.)
        bool synchronousShaderCompilations = false;

        // (macOS only -- ignored on iOS). Override m_platformFeatures.supportsRasterOrdering to
        // false, forcing us to always render in atomic mode.
        bool disableFramebufferReads = false;
    };

    static std::unique_ptr<PLSRenderContext> MakeContext(id<MTLDevice>, const ContextOptions&);

    static std::unique_ptr<PLSRenderContext> MakeContext(id<MTLDevice> gpu)
    {
        return MakeContext(gpu, ContextOptions());
    }

    ~PLSRenderContextMetalImpl() override;

    id<MTLDevice> gpu() const { return m_gpu; }

    rcp<PLSRenderTargetMetal> makeRenderTarget(MTLPixelFormat, uint32_t width, uint32_t height);

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType, RenderBufferFlags, size_t) override;

    rcp<PLSTexture> makeImageTexture(uint32_t width,
                                     uint32_t height,
                                     uint32_t mipLevelCount,
                                     const uint8_t imageDataRGBA[]) override;

    // Atomic mode requires a barrier between overlapping draws. We have to implement this barrier
    // in various different ways, depending on which hardware we're on.
    enum class AtomicBarrierType
    {
        // The hardware supports a normal fragment-fragment memory barrier. (Not supported on
        // Apple-Silicon).
        memoryBarrier,

        // Apple Silicon is very fast at raster ordering, and doesn't support fragment-fragment
        // memory barriers anyway, so on this hardware we just use raster order groups in atomic
        // mode.
        rasterOrderGroup,

        // On very old hardware that can't support barriers, we just take a sledge hammer and break
        // the entire render pass between overlapping draws.
        // TODO: Is there a lighter way to accomplish this?
        renderPassBreak,
    };

    struct MetalFeatures
    {
        AtomicBarrierType atomicBarrierType = AtomicBarrierType::renderPassBreak;
    };

    const MetalFeatures& metalFeatures() const { return m_metalFeatures; }

protected:
    PLSRenderContextMetalImpl(id<MTLDevice>, const ContextOptions&);

    std::unique_ptr<BufferRing> makeUniformBufferRing(size_t capacityInBytes) override;
    std::unique_ptr<BufferRing> makeStorageBufferRing(size_t capacityInBytes,
                                                      StorageBufferStructure) override;
    std::unique_ptr<BufferRing> makeVertexBufferRing(size_t capacityInBytes) override;
    std::unique_ptr<BufferRing> makeTextureTransferBufferRing(size_t capacityInBytes) override;

private:
    // Renders paths to the main render target.
    class DrawPipeline;

    void resizeGradientTexture(uint32_t width, uint32_t height) override;
    void resizeTessellationTexture(uint32_t width, uint32_t height) override;

    // Obtains an exclusive lock on the next buffer ring index, potentially blocking until the GPU
    // has finished rendering with it. This ensures it is safe for the CPU to begin modifying the
    // next buffers in our rings.
    void prepareToMapBuffers() override;

    // Creates a MTLRenderCommandEncoder and sets the common state for PLS draws.
    id<MTLRenderCommandEncoder> makeRenderPassForDraws(const gpu::FlushDescriptor&,
                                                       MTLRenderPassDescriptor*,
                                                       id<MTLCommandBuffer>,
                                                       gpu::ShaderMiscFlags baselineMiscFlags);

    // Returns the specific DrawPipeline for the given feature set, if it has been compiled. If it
    // has not finished compiling yet, this method may return a (potentially slower) DrawPipeline
    // that can draw a superset of the given features.
    const DrawPipeline* findCompatibleDrawPipeline(gpu::DrawType,
                                                   gpu::ShaderFeatures,
                                                   gpu::InterlockMode,
                                                   gpu::ShaderMiscFlags);

    void flush(const FlushDescriptor&) override;

    const ContextOptions m_contextOptions;
    const id<MTLDevice> m_gpu;

    MetalFeatures m_metalFeatures;
    std::unique_ptr<BackgroundShaderCompiler> m_backgroundShaderCompiler;
    id<MTLLibrary> m_plsPrecompiledLibrary; // Many shaders come precompiled in a static library.

    // Renders color ramps to the gradient texture.
    class ColorRampPipeline;
    std::unique_ptr<ColorRampPipeline> m_colorRampPipeline;
    id<MTLTexture> m_gradientTexture = nullptr;

    // Renders tessellated vertices to the tessellation texture.
    class TessellatePipeline;
    std::unique_ptr<TessellatePipeline> m_tessPipeline;
    id<MTLBuffer> m_tessSpanIndexBuffer = nullptr;
    id<MTLTexture> m_tessVertexTexture = nullptr;

    std::map<uint32_t, std::unique_ptr<DrawPipeline>> m_drawPipelines;

    // Vertex/index buffers for drawing path patches.
    id<MTLBuffer> m_pathPatchVertexBuffer;
    id<MTLBuffer> m_pathPatchIndexBuffer;

    // Vertex/index buffers for drawing image rects. (gpu::InterlockMode::atomics only.)
    id<MTLBuffer> m_imageRectVertexBuffer;
    id<MTLBuffer> m_imageRectIndexBuffer;

    // Locks buffer contents until the GPU has finished rendering with them. Prevents the CPU from
    // overriding data before the GPU is done with it.
    std::mutex m_bufferRingLocks[kBufferRingSize];
    int m_bufferRingIdx = 0;
};
} // namespace rive::gpu
