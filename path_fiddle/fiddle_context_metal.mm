#include "fiddle_context.hpp"

#include "rive/pls/pls_renderer.hpp"
#include "rive/pls/gl/pls_render_context_gl_impl.hpp"
#include "rive/pls/gl/pls_render_target_gl.hpp"
#include "rive/pls/metal/pls_render_context_metal_impl.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include "GLFW/glfw3.h"
#include <GLFW/glfw3native.h>

using namespace rive;
using namespace rive::pls;

class FiddleContextMetalPLS : public FiddleContext
{
public:
    FiddleContextMetalPLS(FiddleContextOptions options) : m_options(options)
    {
        if (m_options.synchronousShaderCompilations)
        {
            // Turn on synchronous shader compilations to ensure deterministic rendering and to make
            // sure we test every unique shader.
            m_plsContext->static_impl_cast<rive::pls::PLSRenderContextMetalImpl>()
                ->enableSynchronousShaderCompilations(true);
        }
        printf("==== MTLDevice: %s ====\n", m_gpu.name.UTF8String);
    }

    float dpiScale(GLFWwindow* window) const override
    {
        NSWindow* nsWindow = glfwGetCocoaWindow(window);
        return m_options.retinaDisplay ? nsWindow.backingScaleFactor : 1;
    }

    Factory* factory() override { return m_plsContext.get(); }

    rive::pls::PLSRenderContext* plsContextOrNull() override { return m_plsContext.get(); }

    void onSizeChanged(GLFWwindow* window, int width, int height) override
    {
        NSWindow* nsWindow = glfwGetCocoaWindow(window);
        NSView* view = [nsWindow contentView];
        view.wantsLayer = YES;

        m_swapchain = [CAMetalLayer layer];
        m_swapchain.device = m_gpu;
        m_swapchain.opaque = YES;
        m_swapchain.framebufferOnly = YES;
        m_swapchain.pixelFormat = MTLPixelFormatBGRA8Unorm;
        m_swapchain.contentsScale = dpiScale(window);
        m_swapchain.displaySyncEnabled = NO;
        view.layer = m_swapchain;

        auto plsContextImpl = m_plsContext->static_impl_cast<PLSRenderContextMetalImpl>();
        m_renderTarget = plsContextImpl->makeRenderTarget(MTLPixelFormatBGRA8Unorm, width, height);
        m_pixelReadBuff = nil;
    }

    void toggleZoomWindow() override {}

    std::unique_ptr<Renderer> makeRenderer(int width, int height) override
    {
        return std::make_unique<PLSRenderer>(m_plsContext.get());
    }

    void begin(PLSRenderContext::FrameDescriptor&& frameDescriptor) override
    {
        m_surface = [m_swapchain nextDrawable];
        assert(m_surface.texture.width == m_renderTarget->width());
        assert(m_surface.texture.height == m_renderTarget->height());
        m_renderTarget->setTargetTexture(m_surface.texture);
        frameDescriptor.renderTarget = m_renderTarget;
        m_plsContext->beginFrame(std::move(frameDescriptor));
    }

    void end(GLFWwindow* window, std::vector<uint8_t>* pixelData) final
    {
        m_plsContext->flush();

        if (pixelData != nil)
        {
            // Read back pixels from the framebuffer!
            size_t w = m_renderTarget->width();
            size_t h = m_renderTarget->height();

            // Create a buffer to receive the pixels.
            if (m_pixelReadBuff == nil)
            {
                m_pixelReadBuff = [m_gpu newBufferWithLength:h * w * 4
                                                     options:MTLResourceStorageModeShared];
            }
            assert(m_pixelReadBuff.length == h * w * 4);

            id<MTLCommandBuffer> commandBuffer = [m_queue commandBuffer];
            id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];

            // Blit the framebuffer into m_pixelReadBuff.
            [blitEncoder copyFromTexture:m_renderTarget->targetTexture()
                             sourceSlice:0
                             sourceLevel:0
                            sourceOrigin:MTLOriginMake(0, 0, 0)
                              sourceSize:MTLSizeMake(w, h, 1)
                                toBuffer:m_pixelReadBuff
                       destinationOffset:0
                  destinationBytesPerRow:w * 4
                destinationBytesPerImage:h * w * 4];

            [blitEncoder endEncoding];
            [commandBuffer commit];
            [commandBuffer waitUntilCompleted];

            // Copy the image data from m_pixelReadBuff to pixelData.
            pixelData->resize(h * w * 4);
            const uint8_t* contents = reinterpret_cast<const uint8_t*>(m_pixelReadBuff.contents);
            const size_t rowBytes = w * 4;
            for (size_t y = 0; y < h; ++y)
            {
                // Flip Y.
                const uint8_t* src = &contents[(h - y - 1) * w * 4];
                uint8_t* dst = &(*pixelData)[y * w * 4];
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

        id<MTLCommandBuffer> commandBuffer = [m_queue commandBuffer];
        [commandBuffer presentDrawable:m_surface];
        [commandBuffer commit];
    }

private:
    const FiddleContextOptions m_options;
    id<MTLDevice> m_gpu = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> m_queue = [m_gpu newCommandQueue];
    id<CAMetalDrawable> m_surface = nil;
    std::unique_ptr<PLSRenderContext> m_plsContext =
        PLSRenderContextMetalImpl::MakeContext(m_gpu, m_queue);
    CAMetalLayer* m_swapchain;
    rcp<PLSRenderTargetMetal> m_renderTarget;
    id<MTLBuffer> m_pixelReadBuff;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeMetalPLS(FiddleContextOptions options)
{
    return std::make_unique<FiddleContextMetalPLS>(options);
}
