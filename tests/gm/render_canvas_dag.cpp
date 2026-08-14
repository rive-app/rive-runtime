/*
 * Copyright 2026 Rive
 *
 * A canvas sampling another canvas replays after its writer regardless of
 * record order, so the reversed recording must match the in-order one. The
 * cycle GM pins the demoted back edge to previous-frame sampling.
 */

#include "gm.hpp"
#include "gmutils.hpp"

#if defined(WITH_RIVE_SCRIPTING) && defined(RIVE_CANVAS)

#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;

namespace
{
// DeferredFrameSink over the GM harness. The harness frame is already open,
// so the first sink action flushes it and later frames resume with preserve.
class DagGMSink : public rive::cmd::DeferredFrameSink
{
public:
    DagGMSink(RenderContext* rc,
              const RenderContext::FrameDescriptor& mainDesc) :
        m_rc(rc), m_mainDesc(mainDesc)
    {}

    Factory* factory() override { return TestingWindow::Get()->factory(); }

    // The GM harness owns one main render target.
    Renderer* beginScreenFrame(uint64_t target) override
    {
        assert(target == 0);
        flushOpenFrame();
        auto d = m_mainDesc;
        d.loadAction = LoadAction::preserveRenderTarget;
        m_rc->beginFrame(std::move(d));
        m_frameOpen = true;
        m_screen = std::make_unique<RiveRenderer>(m_rc);
        return m_screen.get();
    }

    Renderer* beginCanvasContent(RenderCanvas* canvas,
                                 uint32_t clearColor) override
    {
        flushOpenFrame();
        m_activeCanvas = canvas;
        auto d = m_mainDesc;
        d.renderTargetWidth = canvas->width();
        d.renderTargetHeight = canvas->height();
        d.loadAction = LoadAction::clear;
        d.clearColor = clearColor;
        m_rc->beginFrame(std::move(d));
        m_frameOpen = true;
        m_canvasRenderer = std::make_unique<RiveRenderer>(m_rc);
        return m_canvasRenderer.get();
    }

    void endCanvasContent() override
    {
        if (m_activeCanvas == nullptr)
        {
            return;
        }
        TestingWindow::Get()->flushPLSContext(m_activeCanvas->renderTarget());
        m_frameOpen = false;
        m_canvasRenderer = nullptr;
        m_activeCanvas = nullptr;
    }

private:
    // The harness (or the previous replay) leaves the main frame open.
    void flushOpenFrame()
    {
        if (!m_flushedHarnessFrame || m_frameOpen)
        {
            TestingWindow::Get()->flushPLSContext();
            m_flushedHarnessFrame = true;
            m_frameOpen = false;
        }
    }

    RenderContext* m_rc;
    RenderContext::FrameDescriptor m_mainDesc;
    bool m_flushedHarnessFrame = false;
    bool m_frameOpen = false;
    std::unique_ptr<RiveRenderer> m_screen;
    std::unique_ptr<RiveRenderer> m_canvasRenderer;
    RenderCanvas* m_activeCanvas = nullptr;
};

rcp<RenderPath> ovalPath(rive::cmd::DeferredSession& session, AABB bounds)
{
    RawPath raw;
    raw.addOval(bounds);
    return session.makeRenderPath(raw, FillRule::nonZero);
}

rcp<RenderPaint> solidPaint(rive::cmd::DeferredSession& session, ColorInt color)
{
    auto paint = session.makeRenderPaint();
    paint->color(color);
    return paint;
}

void drawCanvasImage(Renderer* r,
                     RenderCanvas* canvas,
                     float x,
                     float y,
                     bool flip)
{
    r->save();
    r->translate(x, y);
    if (flip)
    {
        r->translate(0, static_cast<float>(canvas->height()));
        r->scale(1, -1);
    }
    r->drawImage(canvas->renderImage(),
                 {.filter = ImageFilter::nearest},
                 BlendMode::srcOver,
                 1.0f);
    r->restore();
}

void replayFrameThroughGM(rive::cmd::DeferredSession& session,
                          rive::cmd::DeferredReplayer& replayer,
                          RenderContext* rc,
                          const RenderContext::FrameDescriptor& mainDesc)
{
    rive::cmd::DeferredFrame frame = rive::cmd::snapshotFrame(session);
    session.resetFrame();
    DagGMSink sink(rc, mainDesc);
    replayer.replayFrame(frame, sink);
}
} // namespace

// Canvas B samples canvas A; reversed records B's bracket first. Both GMs
// must produce identical pixels: the schedule, not record order, decides.
class CanvasDagChainGM : public GM
{
public:
    CanvasDagChainGM(bool reversed) : GM(256, 256), m_reversed(reversed) {}

    ColorInt clearColor() const override { return 0xff202028; }

    void onDraw(Renderer*) override
    {
        auto rc = TestingWindow::Get()->renderContext();
        if (!rc)
        {
            return;
        }
        auto canvasA = rc->makeRenderCanvas(128, 128);
        auto canvasB = rc->makeRenderCanvas(128, 128);
        if (!canvasA || !canvasB)
        {
            return;
        }
        auto mainDesc = rc->frameDescriptor();
        bool flip = rc->platformFeatures().framebufferBottomUp;

        rive::cmd::DeferredSession session(rive::ore::ReplayCaps{});
        rive::cmd::DeferredReplayer replayer;
        auto green = solidPaint(session, 0xff30c060);
        auto orange = solidPaint(session, 0xffe08830);
        auto circle = ovalPath(session, {24, 24, 104, 104});
        auto dot = ovalPath(session, {8, 8, 40, 40});

        auto recordA = [&]() {
            Renderer* a = session.beginCanvasContent(canvasA.get(), 0xff103050);
            a->drawPath(circle.get(), green.get());
            session.endCanvasContent(canvasA.get());
        };
        auto recordB = [&]() {
            // B composites A, then draws its own dot on top.
            Renderer* b = session.beginCanvasContent(canvasB.get(), 0xff501030);
            drawCanvasImage(b, canvasA.get(), 0, 0, flip);
            b->drawPath(dot.get(), orange.get());
            session.endCanvasContent(canvasB.get());
        };
        if (m_reversed)
        {
            recordB();
            recordA();
        }
        else
        {
            recordA();
            recordB();
        }
        auto screen = session.makeScreenRenderer();
        drawCanvasImage(screen.get(), canvasA.get(), 0, 64, flip);
        drawCanvasImage(screen.get(), canvasB.get(), 128, 64, flip);

        replayFrameThroughGM(session, replayer, rc, mainDesc);
    }

private:
    bool m_reversed;
};

GMREGISTER(canvas_dag_chain, return new CanvasDagChainGM(false))
GMREGISTER(canvas_dag_chain_reversed, return new CanvasDagChainGM(true))

// A samples B while B samples A. The demoted back edge samples the previous
// frame by contract: frame two must show frame one's content crossed over,
// deterministic because frame one seeded both canvases.
class CanvasDagCycleGM : public GM
{
public:
    CanvasDagCycleGM() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xff202028; }

    void onDraw(Renderer*) override
    {
        auto rc = TestingWindow::Get()->renderContext();
        if (!rc)
        {
            return;
        }
        auto canvasA = rc->makeRenderCanvas(128, 128);
        auto canvasB = rc->makeRenderCanvas(128, 128);
        if (!canvasA || !canvasB)
        {
            return;
        }
        auto mainDesc = rc->frameDescriptor();
        bool flip = rc->platformFeatures().framebufferBottomUp;

        rive::cmd::DeferredSession session(rive::ore::ReplayCaps{});
        rive::cmd::DeferredReplayer replayer;

        // Frame one: seed A green, B orange, no cross sampling.
        {
            auto green = solidPaint(session, 0xff30c060);
            auto orange = solidPaint(session, 0xffe08830);
            auto circle = ovalPath(session, {24, 24, 104, 104});
            Renderer* a = session.beginCanvasContent(canvasA.get(), 0xff103050);
            a->drawPath(circle.get(), green.get());
            session.endCanvasContent(canvasA.get());
            Renderer* b = session.beginCanvasContent(canvasB.get(), 0xff501030);
            b->drawPath(circle.get(), orange.get());
            session.endCanvasContent(canvasB.get());
            replayFrameThroughGM(session, replayer, rc, mainDesc);
        }

        // Frame two: each canvas samples the other shrunken, then the screen
        // shows both. The back edge sees frame one's pixels.
        {
            auto white = solidPaint(session, 0xffffffff);
            auto dot = ovalPath(session, {4, 4, 24, 24});
            Renderer* a = session.beginCanvasContent(canvasA.get(), 0xff103050);
            a->save();
            a->scale(0.5f, 0.5f);
            drawCanvasImage(a, canvasB.get(), 0, 0, flip);
            a->restore();
            a->drawPath(dot.get(), white.get());
            session.endCanvasContent(canvasA.get());

            Renderer* b = session.beginCanvasContent(canvasB.get(), 0xff501030);
            b->save();
            b->scale(0.5f, 0.5f);
            drawCanvasImage(b, canvasA.get(), 0, 0, flip);
            b->restore();
            b->drawPath(dot.get(), white.get());
            session.endCanvasContent(canvasB.get());

            auto screen = session.makeScreenRenderer();
            drawCanvasImage(screen.get(), canvasA.get(), 0, 64, flip);
            drawCanvasImage(screen.get(), canvasB.get(), 128, 64, flip);
            replayFrameThroughGM(session, replayer, rc, mainDesc);
        }
    }
};

GMREGISTER(canvas_dag_cycle, return new CanvasDagCycleGM())

#else

// Canvas or scripting disabled: nothing to register.

#endif
