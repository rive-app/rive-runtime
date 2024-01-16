/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls_render_context_helper_impl.hpp"
#include <map>
#include <mutex>

#ifndef RIVE_OBJC_NOP
#import <Metal/Metal.h>
#endif

namespace rive::pls
{
class BackgroundShaderCompiler;

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
                         uint32_t width,
                         uint32_t height,
                         const PlatformFeatures&);

    const MTLPixelFormat m_pixelFormat;
    id<MTLTexture> m_targetTexture;
    id<MTLTexture> m_coverageMemorylessTexture;
    id<MTLTexture> m_originalDstColorMemorylessTexture;
    id<MTLTexture> m_clipMemorylessTexture;
};

// Metal backend implementation of PLSRenderContextImpl.
class PLSRenderContextMetalImpl : public PLSRenderContextHelperImpl
{
public:
    static std::unique_ptr<PLSRenderContext> MakeContext(id<MTLDevice>, id<MTLCommandQueue>);
    ~PLSRenderContextMetalImpl() override;

    id<MTLDevice> gpu() const { return m_gpu; }

    rcp<PLSRenderTargetMetal> makeRenderTarget(MTLPixelFormat, uint32_t width, uint32_t height);

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType, RenderBufferFlags, size_t) override;

    rcp<PLSTexture> makeImageTexture(uint32_t width,
                                     uint32_t height,
                                     uint32_t mipLevelCount,
                                     const uint8_t imageDataRGBA[]) override;

    // Wait for shaders to compile inline with rendering (causing jank), instead of compiling in a
    // background thread asynchronously, and using alternative compatible shaders until the
    // compilations complete. This is mostly intended for testing.
    void enableSynchronousShaderCompilations(bool enabled)
    {
        m_shouldWaitForShaderCompilations = enabled;
    }

protected:
    PLSRenderContextMetalImpl(id<MTLDevice>, id<MTLCommandQueue>);

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

    // Returns the specific DrawPipeline for the given feature set, if it has been compiled. If it
    // has not finished compiling yet, returns a (potentially slower) draw pipeline that can draw a
    // superset of the given features.
    const DrawPipeline* findCompatibleDrawPipeline(pls::DrawType, pls::ShaderFeatures);

    void flush(const FlushDescriptor&) override;

    const id<MTLDevice> m_gpu;
    const id<MTLCommandQueue> m_queue;

    id<MTLLibrary> m_plsPrecompiledLibrary;
    std::unique_ptr<BackgroundShaderCompiler> m_backgroundShaderCompiler;
    bool m_shouldWaitForShaderCompilations = false;

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
    id<MTLBuffer> m_pathPatchVertexBuffer;
    id<MTLBuffer> m_pathPatchIndexBuffer;

    // Locks buffer contents until the GPU has finished rendering with them. Prevents the CPU from
    // overriding data before the GPU is done with it.
    std::mutex m_bufferRingLocks[kBufferRingSize];
    int m_bufferRingIdx = 0;

    // Stashed active command buffer between logical flushes.
    id<MTLCommandBuffer> m_currentCommandBuffer = nullptr;
};
} // namespace rive::pls
