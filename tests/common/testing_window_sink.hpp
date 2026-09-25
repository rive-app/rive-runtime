/*
 * Copyright 2026 Rive
 */

#pragma once

#ifdef RIVE_CANVAS

#include "common/testing_window.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_context_impl.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include <assert.h>
#include <memory>

// DeferredFrameSink over TestingWindow. Leaves the screen frame open for the
// caller to present via endFrame.
class TestingWindowFrameSink : public rive::cmd::DeferredFrameSink
{
public:
    using FrameOptions = TestingWindow::FrameOptions;

    explicit TestingWindowFrameSink(const FrameOptions& options) :
        m_rc(TestingWindow::Get()->renderContext()), m_options(options)
    {}

    rive::Factory* factory() override
    {
        return TestingWindow::Get()->factory();
    }

    rive::gpu::RenderContext* renderContext() override { return m_rc; }

    // The tools render into the one TestingWindow, so there is a single
    // target.
    rive::Renderer* beginScreenFrame(uint64_t target) override
    {
        assert(target == 0);
        m_screen = TestingWindow::Get()->beginFrame(m_options);
        return m_screen.get();
    }
    void beginOreFrame() override { TestingWindow::Get()->beginOreFrame(); }
    void endOreFrame() override { TestingWindow::Get()->endOreFrame(); }

    rive::Renderer* beginCanvasContent(rive::gpu::RenderCanvas* canvas,
                                       uint32_t clearColor) override
    {
        m_activeCanvas = canvas;
        rive::gpu::RenderContext::FrameDescriptor d{};
        d.renderTargetWidth = canvas->width();
        d.renderTargetHeight = canvas->height();
        d.loadAction = rive::gpu::LoadAction::clear;
        d.clearColor = clearColor;
        // Canvas content is the same frame as the screen content around it, so
        // it has to be drawn under the same rules.
        TestingWindow::FrameMode mode =
            TestingWindow::Get()->canvasFrameMode(m_options);
        d.msaaSampleCount = mode.msaaSampleCount;
        d.disableRasterOrdering = mode.disableRasterOrdering;
        d.clockwiseFillOverride = mode.clockwiseFillOverride;
        d.triangulationThresholds = m_options.triangulationThresholds;
        m_rc->beginFrame(d);
        m_canvasRenderer = std::make_unique<rive::RiveRenderer>(m_rc);
        return m_canvasRenderer.get();
    }
    void endCanvasContent() override
    {
        if (m_activeCanvas == nullptr)
            return;
        // The window's own frame command buffer, so a canvas flush never
        // idles the queue the way the impl's canvas command buffer does.
        TestingWindow::Get()->flushPLSContext(m_activeCanvas->renderTarget());
        m_canvasRenderer.reset();
        m_activeCanvas = nullptr;
    }

private:
    rive::gpu::RenderContext* m_rc;
    FrameOptions m_options;
    std::unique_ptr<rive::Renderer> m_screen;
    std::unique_ptr<rive::RiveRenderer> m_canvasRenderer;
    rive::gpu::RenderCanvas* m_activeCanvas = nullptr;
};

#endif // RIVE_CANVAS
