/*
 * Copyright 2024 Rive
 */

#include "testing_window.hpp"

#if defined(__APPLE__) && !defined(RIVE_UNREAL)

#include "rive/renderer/metal/render_context_metal_impl.h"
#include "rive/renderer/rive_renderer.hpp"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace rive::gpu
{
class TestingWindowMetalTexture : public TestingWindow
{
public:
    TestingWindowMetalTexture(const BackendParams& backendParams)
    {
        RenderContextMetalImpl::ContextOptions metalOptions;
        // Turn on synchronous shader compilations to ensure deterministic
        // rendering and to make sure we test every unique shader.
        metalOptions.shaderCompilationMode =
            ShaderCompilationMode::alwaysSynchronous;
        metalOptions.disableFramebufferReads = backendParams.atomic;
        m_renderContext =
            RenderContextMetalImpl::MakeContext(m_gpu, metalOptions);
        printf("==== MTLDevice: %s ====\n", m_gpu.name.UTF8String);
    }

    rive::Factory* factory() override { return m_renderContext.get(); }

    rive::gpu::RenderContext* renderContext() const override
    {
        return m_renderContext.get();
    }

    std::unique_ptr<rive::Renderer> beginFrame(
        const FrameOptions& options) override
    {
        rive::gpu::RenderContext::FrameDescriptor frameDescriptor = {
            .renderTargetWidth = m_width,
            .renderTargetHeight = m_height,
            .loadAction = options.doClear
                              ? rive::gpu::LoadAction::clear
                              : rive::gpu::LoadAction::preserveRenderTarget,
            .clearColor = options.clearColor,
            .disableRasterOrdering = options.disableRasterOrdering,
            .wireframe = options.wireframe,
            .fillsDisabled = options.fillsDisabled,
            .strokesDisabled = options.strokesDisabled,
            .clockwiseFillOverride = options.clockwiseFillOverride,
            .synthesizedFailureType = options.synthesizedFailureType,
        };
        m_renderContext->beginFrame(frameDescriptor);
        m_flushCommandBuffer = [m_queue commandBuffer];
        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void flushPLSContext(RenderTarget* offscreenRenderTarget) final
    {
        if (!m_renderTarget || m_renderTarget->height() != m_height ||
            m_renderTarget->width() != m_width)
        {
            m_renderTarget =
                m_renderContext->static_impl_cast<RenderContextMetalImpl>()
                    ->makeRenderTarget(
                        MTLPixelFormatBGRA8Unorm, m_width, m_height);
            MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
            desc.pixelFormat = MTLPixelFormatBGRA8Unorm;
            desc.width = m_width;
            desc.height = m_height;
            desc.usage = MTLTextureUsageRenderTarget;
            desc.textureType = MTLTextureType2D;
            desc.mipmapLevelCount = 1;
            desc.storageMode = MTLStorageModePrivate;
            m_renderTarget->setTargetTexture(
                [m_gpu newTextureWithDescriptor:desc]);
            m_pixelReadBuff =
                [m_gpu newBufferWithLength:m_height * m_width * 4
                                   options:MTLResourceStorageModeShared];
        }
        m_renderContext->flush({
            .renderTarget = offscreenRenderTarget != nullptr
                                ? offscreenRenderTarget
                                : m_renderTarget.get(),
            .externalCommandBuffer = (__bridge void*)m_flushCommandBuffer,
        });
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        flushPLSContext(nullptr);

        if (pixelData)
        {
            id<MTLBlitCommandEncoder> blitEncoder =
                [m_flushCommandBuffer blitCommandEncoder];

            // Blit the framebuffer into m_pixelReadBuff.
            [blitEncoder copyFromTexture:m_renderTarget->targetTexture()
                             sourceSlice:0
                             sourceLevel:0
                            sourceOrigin:MTLOriginMake(0, 0, 0)
                              sourceSize:MTLSizeMake(m_width, m_height, 1)
                                toBuffer:m_pixelReadBuff
                       destinationOffset:0
                  destinationBytesPerRow:m_width * 4
                destinationBytesPerImage:m_height * m_width * 4];

            [blitEncoder endEncoding];
        }

        [m_flushCommandBuffer commit];

        if (pixelData)
        {
            [m_flushCommandBuffer waitUntilCompleted];

            // Copy the image data from m_pixelReadBuff to pixelData.
            pixelData->resize(m_height * m_width * 4);
            assert(m_pixelReadBuff.length == pixelData->size());
            const uint8_t* contents =
                reinterpret_cast<const uint8_t*>(m_pixelReadBuff.contents);
            const size_t rowBytes = m_width * 4;
            for (size_t y = 0; y < m_height; ++y)
            {
                // Flip Y.
                const uint8_t* src =
                    &contents[(m_height - y - 1) * m_width * 4];
                uint8_t* dst = &(*pixelData)[y * m_width * 4];
                for (size_t x = 0; x < rowBytes; x += 4)
                {
                    // BGBRA -> RGBA.
                    dst[x + 0] = src[x + 2];
                    dst[x + 1] = src[x + 1];
                    dst[x + 2] = src[x + 0];
                    dst[x + 3] = src[x + 3];
                }
            }
        }

        m_flushCommandBuffer = nil;
    }

private:
    id<MTLDevice> m_gpu = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> m_queue = [m_gpu newCommandQueue];
    std::unique_ptr<RenderContext> m_renderContext;
    rcp<RenderTargetMetal> m_renderTarget;
    id<MTLBuffer> m_pixelReadBuff;
    id<MTLCommandBuffer> m_flushCommandBuffer;
};
}; // namespace rive::gpu

TestingWindow* TestingWindow::MakeMetalTexture(
    const BackendParams& backendParams)
{
    return new rive::gpu::TestingWindowMetalTexture(backendParams);
}

#endif
