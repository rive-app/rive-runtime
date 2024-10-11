/*
 * Copyright 2022 Rive
 */

#include "testing_gl_renderer.hpp"

#ifndef RIVE_TOOLS_NO_GL

#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"

TestingGLRenderer::~TestingGLRenderer() {}

std::unique_ptr<TestingGLRenderer> TestingGLRenderer::Make(
    TestingWindow::RendererFlags rendererFlags)
{
    return TestingGLRenderer::MakePLS(rendererFlags);
}

std::unique_ptr<TestingGLRenderer> TestingGLRenderer::MakePLS(
    TestingWindow::RendererFlags rendererFlags)
{
    class RiveRenderer : public TestingGLRenderer
    {
    public:
        RiveRenderer(TestingWindow::RendererFlags rendererFlags) :
            m_rendererFlags(rendererFlags)
        {
            if (m_rendererFlags & TestingWindow::RendererFlags::useMSAA)
            {
                m_contextOptions.disablePixelLocalStorage = true;
            }
            if (m_rendererFlags &
                TestingWindow::RendererFlags::disableRasterOrdering)
            {
                m_contextOptions.disableFragmentShaderInterlock = true;
            }
        }

        void init(void* getGLProcAddress) override
        {
            m_renderContext =
                rive::gpu::RenderContextGLImpl::MakeContext(m_contextOptions);
        }

        rive::Factory* factory() override { return m_renderContext.get(); }

        std::unique_ptr<rive::Renderer> reset(int width,
                                              int height,
                                              uint32_t targetTextureID) override
        {
            if (targetTextureID == 0)
            {
                // Render directly to the default framebuffer.
                GLint sampleCount;
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glGetIntegerv(GL_SAMPLES, &sampleCount);
                m_renderTarget =
                    rive::make_rcp<rive::gpu::FramebufferRenderTargetGL>(
                        width,
                        height,
                        0,
                        sampleCount);
            }
            else
            {
                // Render to targetTextureID.
                auto renderTarget =
                    rive::make_rcp<rive::gpu::TextureRenderTargetGL>(width,
                                                                     height);
                renderTarget->setTargetTexture(targetTextureID);
                m_renderTarget = std::move(renderTarget);
            }
            return std::make_unique<rive::RiveRenderer>(m_renderContext.get());
        }

        void beginFrame(rive::ColorInt clearColor,
                        bool doClear,
                        bool wireframe) override
        {
            // For testing, reset GPU resources to their initial sizes every
            // frame. This will stress intermediate flushes more, as well as
            // creating more consistency when rendering in multiple threads and
            // a nondeterministic order.
            m_renderContext->releaseResources();

            rive::gpu::RenderContext::FrameDescriptor frameDescriptor = {
                .renderTargetWidth = m_renderTarget->width(),
                .renderTargetHeight = m_renderTarget->height(),
                .loadAction = doClear
                                  ? rive::gpu::LoadAction::clear
                                  : rive::gpu::LoadAction::preserveRenderTarget,
                .clearColor = clearColor,
                .msaaSampleCount =
                    (m_rendererFlags & TestingWindow::RendererFlags::useMSAA)
                        ? 4
                        : 0,
                .disableRasterOrdering =
                    (m_rendererFlags &
                     TestingWindow::RendererFlags::disableRasterOrdering),
                .wireframe = wireframe,
            };
            m_renderContext->beginFrame(frameDescriptor);
        }

        void flush(int dpiScale) override { flushPLSContext(); }

        rive::gpu::RenderContext* renderContext() const override
        {
            return m_renderContext.get();
        }
        rive::gpu::RenderTarget* renderTarget() const override
        {
            return m_renderTarget.get();
        }

        void flushPLSContext() override
        {
            m_renderContext->flush({.renderTarget = m_renderTarget.get()});
        }

    private:
        const TestingWindow::RendererFlags m_rendererFlags;
        rive::gpu::RenderContextGLImpl::ContextOptions m_contextOptions;
        std::unique_ptr<rive::gpu::RenderContext> m_renderContext;
        rive::rcp<rive::gpu::RenderTargetGL> m_renderTarget;
    };
    return std::make_unique<RiveRenderer>(rendererFlags);
}

#endif
