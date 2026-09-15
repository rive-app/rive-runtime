/*
 * Copyright 2026 Rive
 */

#pragma once

#ifdef RIVE_CANVAS

#include "common/testing_window.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"

#include <cassert>
#include <memory>

namespace rive_tests
{
// A DeferredFrameSink that replays a recorded frame against a TestingWindow's
// real render context, offscreen canvas frames included. This is what a test
// needs in order to see the *pixels* a deferred recording produces: the
// GPU-free sinks (deferred_test::TestSink, SerializingFactory) drop canvas
// content instead of rasterizing it.
//
// The window's frame is expected to be already open when the first sink action
// runs -- that frame is flushed and later ones resume with preserve, so the
// caller still ends the frame through the window and reads pixels back the
// usual way.
class TestingWindowFrameSink : public rive::cmd::DeferredFrameSink
{
public:
    TestingWindowFrameSink(
        TestingWindow* window,
        rive::gpu::RenderContext* rc,
        const rive::gpu::RenderContext::FrameDescriptor& mainDesc) :
        m_window(window), m_rc(rc), m_mainDesc(mainDesc)
    {}

    rive::Factory* factory() override { return m_window->factory(); }

    rive::gpu::RenderContext* renderContext() override { return m_rc; }

    // The window owns one main render target.
    rive::Renderer* beginScreenFrame(uint64_t target) override
    {
        assert(target == 0);
        flushOpenFrame();
        auto d = m_mainDesc;
        d.loadAction = rive::gpu::LoadAction::preserveRenderTarget;
        m_rc->beginFrame(std::move(d));
        m_frameOpen = true;
        m_screen = std::make_unique<rive::RiveRenderer>(m_rc);
        return m_screen.get();
    }

    rive::Renderer* beginCanvasContent(rive::gpu::RenderCanvas* canvas,
                                       uint32_t clearColor) override
    {
        flushOpenFrame();
        m_activeCanvas = canvas;
        m_canvasFrames++;
        auto d = m_mainDesc;
        d.renderTargetWidth = canvas->width();
        d.renderTargetHeight = canvas->height();
        d.loadAction = rive::gpu::LoadAction::clear;
        d.clearColor = clearColor;
        m_rc->beginFrame(std::move(d));
        m_frameOpen = true;
        m_canvasRenderer = std::make_unique<rive::RiveRenderer>(m_rc);
        return m_canvasRenderer.get();
    }

    void endCanvasContent() override
    {
        if (m_activeCanvas == nullptr)
        {
            return;
        }
        m_window->flushPLSContext(m_activeCanvas->renderTarget());
        m_frameOpen = false;
        m_canvasRenderer = nullptr;
        m_activeCanvas = nullptr;
    }

    // Offscreen frames opened while replaying, i.e. how many times the content
    // was actually rasterized into a canvas.
    size_t canvasFrames() const { return m_canvasFrames; }

private:
    // The window (or the previous replay) leaves the main frame open.
    void flushOpenFrame()
    {
        if (!m_flushedWindowFrame || m_frameOpen)
        {
            m_window->flushPLSContext();
            m_flushedWindowFrame = true;
            m_frameOpen = false;
        }
    }

    TestingWindow* m_window;
    rive::gpu::RenderContext* m_rc;
    rive::gpu::RenderContext::FrameDescriptor m_mainDesc;
    bool m_flushedWindowFrame = false;
    bool m_frameOpen = false;
    size_t m_canvasFrames = 0;
    std::unique_ptr<rive::RiveRenderer> m_screen;
    std::unique_ptr<rive::RiveRenderer> m_canvasRenderer;
    rive::gpu::RenderCanvas* m_activeCanvas = nullptr;
};
} // namespace rive_tests

#endif // RIVE_CANVAS
