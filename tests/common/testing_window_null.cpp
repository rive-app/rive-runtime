/*
 * Copyright 2025 Rive
 */

#include "testing_window.hpp"

#include "common/render_context_null.hpp"
#include "rive/renderer/rive_renderer.hpp"

namespace rive::gpu
{
class TestingWindowNULL : public TestingWindow
{
public:
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
            .msaaSampleCount = options.forceMSAA ? 4u : 0u,
            .disableRasterOrdering = options.disableRasterOrdering,
            .wireframe = options.wireframe,
            .clockwiseFillOverride = options.clockwiseFillOverride,
            .synthesizedFailureType = options.synthesizedFailureType,
        };
        m_renderContext->beginFrame(frameDescriptor);
        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void flushPLSContext(RenderTarget* renderTarget) final
    {
        rcp<RenderTarget> nullTarget;
        if (renderTarget == nullptr)
        {
            nullTarget = m_renderContext->static_impl_cast<RenderContextNULL>()
                             ->makeRenderTarget(m_width, m_height);
            renderTarget = nullTarget.get();
        }
        m_renderContext->flush({.renderTarget = renderTarget});
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        flushPLSContext(nullptr);

        if (pixelData)
        {
            // Copy the image data from m_pixelReadBuff to pixelData.
            pixelData->clear();
            pixelData->reserve(m_height * m_width * 4);
            for (size_t i = 0; i < m_height * m_width; ++i)
            {
                constexpr static uint8_t magenta[] = {0xff, 0x00, 0xff, 0xff};
                pixelData->insert(pixelData->end(), magenta, magenta + 4);
            }
        }
    }

private:
    std::unique_ptr<RenderContext> m_renderContext =
        RenderContextNULL::MakeContext();
};
}; // namespace rive::gpu

TestingWindow* TestingWindow::MakeNULL()
{
    return new rive::gpu::TestingWindowNULL();
}
