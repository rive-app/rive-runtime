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
    const TestingWindow::BackendParams& backendParams)
{
    class RiveRenderer : public TestingGLRenderer
    {
    public:
        RiveRenderer(const TestingWindow::BackendParams& backendParams) :
            m_backendParams(backendParams)
        {
            m_contextOptions.shaderCompilationMode =
                rive::gpu::ShaderCompilationMode::alwaysSynchronous;
            if (m_backendParams.msaa)
            {
                m_contextOptions.disablePixelLocalStorage = true;
            }
            if (m_backendParams.atomic)
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

        void beginFrame(const TestingWindow::FrameOptions& options) override
        {
            // For testing, reset GPU resources to their initial sizes every
            // frame. This will stress intermediate flushes more, as well as
            // creating more consistency when rendering in multiple threads and
            // a nondeterministic order.
            m_renderContext->releaseResources();

            rive::gpu::RenderContext::FrameDescriptor frameDescriptor = {
                .renderTargetWidth = m_renderTarget->width(),
                .renderTargetHeight = m_renderTarget->height(),
                .loadAction = options.doClear
                                  ? rive::gpu::LoadAction::clear
                                  : rive::gpu::LoadAction::preserveRenderTarget,
                .clearColor = options.clearColor,
                .msaaSampleCount =
                    (m_backendParams.msaa || options.forceMSAA) ? 4 : 0,
                .disableRasterOrdering =
                    m_backendParams.atomic || options.disableRasterOrdering,
                .wireframe = options.wireframe,
                .clockwiseFillOverride =
                    m_backendParams.clockwise || options.clockwiseFillOverride,
                .synthesizeCompilationFailures =
                    options.synthesizeCompilationFailures,
            };
            m_renderContext->beginFrame(frameDescriptor);
        }

        void flush(int dpiScale) override { flushPLSContext(nullptr); }

        rive::gpu::RenderContext* renderContext() const override
        {
            return m_renderContext.get();
        }
        rive::gpu::RenderTarget* renderTarget() const override
        {
            return m_renderTarget.get();
        }

        void flushPLSContext(
            rive::gpu::RenderTarget* offscreenRenderTarget) override
        {
            m_renderContext->flush({
                .renderTarget = offscreenRenderTarget != nullptr
                                    ? offscreenRenderTarget
                                    : m_renderTarget.get(),
            });
        }

    private:
        const TestingWindow::BackendParams m_backendParams;
        rive::gpu::RenderContextGLImpl::ContextOptions m_contextOptions;
        std::unique_ptr<rive::gpu::RenderContext> m_renderContext;
        rive::rcp<rive::gpu::RenderTargetGL> m_renderTarget;
    };
    return std::make_unique<RiveRenderer>(backendParams);
}

#endif
