#include "fiddle_context.hpp"

#include "rive/pls/pls_factory.hpp"
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
    FiddleContextMetalPLS() { printf("==== MTLDevice: %s ====\n", m_gpu.name.UTF8String); }

    float dpiScale() const override
    {
        NSWindow* nsWindow = glfwGetCocoaWindow(g_window);
        return nsWindow.screen.backingScaleFactor;
    }

    std::unique_ptr<Factory> makeFactory() override { return std::make_unique<PLSFactory>(); }

    void onSizeChanged(int width, int height) override
    {
        NSWindow* nsWindow = glfwGetCocoaWindow(g_window);
        nsWindow.contentView.wantsLayer = YES;
        float backingScaleFactor = [[nsWindow screen] backingScaleFactor];

        m_swapchain = [CAMetalLayer layer];
        m_swapchain.device = m_gpu;
        m_swapchain.opaque = YES;
        m_swapchain.framebufferOnly = YES;
        m_swapchain.pixelFormat = MTLPixelFormatBGRA8Unorm;
        m_swapchain.contentsScale = backingScaleFactor;
        m_swapchain.displaySyncEnabled = NO;
        nsWindow.contentView.layer = m_swapchain;

        auto plsContextImpl = m_plsContext->static_impl_cast<PLSRenderContextMetalImpl>();
        m_renderTarget = plsContextImpl->makeRenderTarget(MTLPixelFormatBGRA8Unorm, width, height);
    }

    void toggleZoomWindow() override {}

    std::unique_ptr<Renderer> makeRenderer(int width, int height) override
    {
        return std::make_unique<PLSRenderer>(m_plsContext.get());
    }

    void begin() override
    {
        m_surface = [m_swapchain nextDrawable];
        m_renderTarget->setTargetTexture(m_surface.texture);

        PLSRenderContext::FrameDescriptor frameDescriptor;
        frameDescriptor.renderTarget = m_renderTarget;
        frameDescriptor.clearColor = 0xff404040;
        frameDescriptor.wireframe = g_wireframe;
        frameDescriptor.fillsDisabled = g_disableFill;
        frameDescriptor.strokesDisabled = g_disableStroke;
        m_plsContext->beginFrame(std::move(frameDescriptor));
    }

    void end() override
    {
        m_plsContext->flush();

        id<MTLCommandBuffer> commandBuffer = [m_queue commandBuffer];
        [commandBuffer presentDrawable:m_surface];
        [commandBuffer commit];
    }

    void shrinkGPUResourcesToFit() final { m_plsContext->shrinkGPUResourcesToFit(); }

private:
    id<MTLDevice> m_gpu = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> m_queue = [m_gpu newCommandQueue];
    id<CAMetalDrawable> m_surface = nil;
    std::unique_ptr<PLSRenderContext> m_plsContext =
        PLSRenderContextMetalImpl::MakeContext(m_gpu, m_queue);
    CAMetalLayer* m_swapchain;
    rcp<PLSRenderTargetMetal> m_renderTarget;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeMetalPLS()
{
    return std::make_unique<FiddleContextMetalPLS>();
}
