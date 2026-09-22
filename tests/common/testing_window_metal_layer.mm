/*
 * Copyright 2026 Rive
 */

#include "testing_window.hpp"

#if defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)

#include "common/rive_ios_app.hpp"
#include "rive/renderer/metal/render_context_metal_impl.h"
#include "rive/renderer/rive_renderer.hpp"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace rive::gpu
{
// Metal requires an aligned row stride for texture-to-buffer blits; 256
// satisfies every format's minimumLinearTextureAlignment on Apple GPUs.
static constexpr size_t kBlitRowAlignment = 256;

class TestingWindowMetalLayer : public TestingWindow
{
public:
    TestingWindowMetalLayer(const BackendParams& backendParams,
                            void* platformWindow) :
        m_layer((__bridge CAMetalLayer*)platformWindow)
    {
        RenderContextMetalImpl::ContextOptions metalOptions;
        metalOptions.shaderCompilationMode =
            backendParams.shaderCompilationMode;
        metalOptions.disableFramebufferReads = backendParams.atomic;
        m_renderContext =
            RenderContextMetalImpl::MakeContext(m_gpu, metalOptions);
        m_renderContext->static_impl_cast<RenderContextMetalImpl>()
            ->setCommandQueue(m_queue);

        m_layer.device = m_gpu;
        m_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        m_layer.framebufferOnly = NO;

        syncSizeFromLayer();

        printf("==== MTLDevice: %s (%ux%u) ====\n",
               m_gpu.name.UTF8String,
               m_width,
               m_height);
    }

    rive::Factory* factory() override { return m_renderContext.get(); }

    rive::gpu::RenderContext* renderContext() const override
    {
        return m_renderContext.get();
    }

    void* getOreContext() const override
    {
        return m_renderContext->getOreContext();
    }

    void beginOreFrame() override
    {
        auto oreContext =
            static_cast<rive::ore::Context*>(m_renderContext->getOreContext());
        oreContext->beginFrame({});
    }

    void endOreFrame() override
    {
        auto oreContext =
            static_cast<rive::ore::Context*>(m_renderContext->getOreContext());
        oreContext->endFrame();
    }

    void resize(int width, int height) override
    {
        TestingWindow::resize(width, height);
        m_layer.drawableSize = CGSizeMake(width, height);
    }

    bool consumeInputEvent(InputEventData& eventData) override
    {
        return rive_ios_app_poll_input_event(eventData);
    }

    bool shouldQuit() const override { return rive_ios_app_should_quit(); }

    std::unique_ptr<rive::Renderer> beginFrame(
        const FrameOptions& options) override
    {
        rive_ios_app_wait_while_inactive();
        syncSizeFromLayer();
        m_drawable = [m_layer nextDrawable];

        rive::gpu::RenderContext::FrameDescriptor frameDescriptor = {
            .renderTargetWidth = m_width,
            .renderTargetHeight = m_height,
            .loadAction = options.doClear
                              ? rive::gpu::LoadAction::clear
                              : rive::gpu::LoadAction::preserveRenderTarget,
            .clearColor = options.clearColor,
            .disableRasterOrdering = options.disableRasterOrdering,
            .triangulationThresholds = options.triangulationThresholds,
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
        if (!m_renderTarget || m_renderTarget->width() != m_width ||
            m_renderTarget->height() != m_height)
        {
            m_renderTarget =
                m_renderContext->static_impl_cast<RenderContextMetalImpl>()
                    ->makeRenderTarget(
                        MTLPixelFormatBGRA8Unorm, m_width, m_height);
        }
        m_renderTarget->setTargetTexture(m_drawable != nil ? m_drawable.texture
                                                           : nil);

        m_renderContext->flush({
            .renderTarget = offscreenRenderTarget != nullptr
                                ? offscreenRenderTarget
                                : m_renderTarget.get(),
            .externalCommandBuffer = (__bridge void*)m_flushCommandBuffer,
        });
        [m_flushCommandBuffer commit];
        m_flushCommandBuffer = [m_queue commandBuffer];
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        flushPLSContext(nullptr);

        const bool readPixels = pixelData != nullptr && m_drawable != nil;
        const size_t tightRowBytes = m_width * 4;
        const size_t alignedRowBytes =
            (tightRowBytes + kBlitRowAlignment - 1) & ~(kBlitRowAlignment - 1);
        const size_t readBuffLength = alignedRowBytes * m_height;
        if (readPixels)
        {
            if (!m_pixelReadBuff || m_pixelReadBuff.length != readBuffLength)
            {
                m_pixelReadBuff =
                    [m_gpu newBufferWithLength:readBuffLength
                                       options:MTLResourceStorageModeShared];
            }
            id<MTLBlitCommandEncoder> blitEncoder =
                [m_flushCommandBuffer blitCommandEncoder];
            [blitEncoder copyFromTexture:m_drawable.texture
                             sourceSlice:0
                             sourceLevel:0
                            sourceOrigin:MTLOriginMake(0, 0, 0)
                              sourceSize:MTLSizeMake(m_width, m_height, 1)
                                toBuffer:m_pixelReadBuff
                       destinationOffset:0
                  destinationBytesPerRow:alignedRowBytes
                destinationBytesPerImage:readBuffLength];
            [blitEncoder endEncoding];
        }

        if (m_drawable != nil)
        {
            [m_flushCommandBuffer presentDrawable:m_drawable];
        }
        [m_flushCommandBuffer commit];

        if (readPixels)
        {
            [m_flushCommandBuffer waitUntilCompleted];

            pixelData->resize(tightRowBytes * m_height);
            const uint8_t* contents =
                reinterpret_cast<const uint8_t*>(m_pixelReadBuff.contents);
            for (size_t y = 0; y < m_height; ++y)
            {
                const uint8_t* src =
                    &contents[(m_height - y - 1) * alignedRowBytes];
                uint8_t* dst = &(*pixelData)[y * tightRowBytes];
                for (size_t x = 0; x < tightRowBytes; x += 4)
                {
                    dst[x + 0] = src[x + 2];
                    dst[x + 1] = src[x + 1];
                    dst[x + 2] = src[x + 0];
                    dst[x + 3] = src[x + 3];
                }
            }
        }

        m_drawable = nil;
        m_flushCommandBuffer = nil;
    }

    void* metalQueue() const override { return (__bridge void*)m_queue; }

    void* getCurrentCommandBuffer() const override
    {
        return (__bridge void*)m_queue;
    }

private:
    void syncSizeFromLayer()
    {
        CGSize size = m_layer.drawableSize;
        m_width = static_cast<uint32_t>(size.width);
        m_height = static_cast<uint32_t>(size.height);
    }

    CAMetalLayer* m_layer;
    id<MTLDevice> m_gpu = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> m_queue = [m_gpu newCommandQueue];
    std::unique_ptr<RenderContext> m_renderContext;
    rcp<RenderTargetMetal> m_renderTarget;
    id<CAMetalDrawable> m_drawable = nil;
    id<MTLBuffer> m_pixelReadBuff;
    id<MTLCommandBuffer> m_flushCommandBuffer;
};
}; // namespace rive::gpu

TestingWindow* TestingWindow::MakeMetalLayer(const BackendParams& backendParams,
                                             void* platformWindow)
{
    if (platformWindow == nullptr)
    {
        fprintf(stderr, "MakeMetalLayer requires a CAMetalLayer.\n");
        return nullptr;
    }
    return new rive::gpu::TestingWindowMetalLayer(backendParams,
                                                  platformWindow);
}

#endif
