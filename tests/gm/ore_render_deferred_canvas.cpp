/*
 * Copyright 2026 Rive
 *
 * Ore clears a canvas and the 2D screen draws that canvas image, immediately
 * and through a DeferredSession drained by the shared DeferredReplayer. The
 * canvas image travels the stream as a shared id, never a pointer. The goldens
 * must be byte identical.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"
#if ORE_GM_HAS_BACKEND
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;
#if ORE_GM_HAS_BACKEND
using namespace rive::ore;
// Disambiguates from rive::cmd.

// The GM is handed an already open screen renderer, so beginScreenFrame just
// returns it.
class GMCanvasSink : public rive::cmd::DeferredFrameSink
{
public:
    GMCanvasSink(rive::gpu::RenderContext* rc,
                 rive::Renderer* screen,
                 ore_gm::OreGMContext* oreCtx) :
        m_rc(rc), m_screen(screen), m_ore(oreCtx)
    {}
    rive::Factory* factory() override
    {
        return TestingWindow::Get()->factory();
    }
    // The GM hands over one already open screen renderer, so there is nothing
    // to dispatch on.
    rive::Renderer* beginScreenFrame(uint64_t target) override
    {
        assert(target == 0);
        return m_screen;
    }
    void beginOreFrame() override { m_ore->beginFrame(m_rc); }
    void endOreFrame() override { m_ore->endFrame(m_rc); }
    void afterOreFrame() override { ore_gm::invalidateGLStateAfterOre(m_rc); }

private:
    rive::gpu::RenderContext* m_rc;
    rive::Renderer* m_screen;
    ore_gm::OreGMContext* m_ore;
};
#endif

class RenderDeferredCanvasGM : public GM
{
public:
    RenderDeferredCanvasGM(bool deferred) : GM(256, 256), m_deferred(deferred)
    {}

    ColorInt clearColor() const override { return 0xff202028; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
        {
            return;
        }

#if ORE_GM_HAS_BACKEND
        auto& realCtx = *renderContext->getOreContext();
        auto canvas = renderContext->makeRenderCanvas(200, 200);
        if (!canvas)
        {
            return;
        }

        ImageSampler sampler{};
        sampler.filter = ImageFilter::nearest;

        auto recordClear = [&](Context& ctx, TextureView* view) {
            ColorAttachment ca{};
            ca.view = view;
            ca.loadOp = LoadOp::clear;
            ca.storeOp = StoreOp::store;
            ca.clearColor = {0.10f, 0.70f, 0.55f, 1.0f};
            RenderPassDesc rp{};
            rp.colorAttachments[0] = ca;
            rp.colorCount = 1;
            auto pass = ctx.beginRenderPass(rp);
            pass->setViewport(0, 0, 200, 200);
            pass->finish();
        };
        auto drawCanvasToScreen = [&](Renderer* r) {
            r->save();
            r->translate(28, 28);
            r->drawImage(canvas->renderImage(),
                         sampler,
                         BlendMode::srcOver,
                         1.0f);
            r->restore();
        };

        if (m_deferred)
        {
            // Same DeferredReplayer the goldens host and editor use. The marker
            // orders the Ore replay before the screen draw.
            rive::cmd::DeferredSession session(
                rive::ore::ReplayCaps::from(realCtx));

            auto view = session.oreContext().wrapCanvasTexture(canvas.get());
            recordClear(session.oreContext(), view.get());
            session.recordOreReplayMarker();

            auto dr = session.makeScreenRenderer();
            drawCanvasToScreen(dr.get());

            // Snapshot replay is the same path a threaded consumer takes.
            rive::cmd::DeferredFrame frame = rive::cmd::snapshotFrame(session);
            GMCanvasSink sink(renderContext, originalRenderer, &m_ore);
            rive::cmd::DeferredReplayer replayer;
            replayer.replayFrame(frame, sink);
        }
        else
        {
            auto view = realCtx.wrapCanvasTexture(canvas.get());
            m_ore.beginFrame(renderContext);
            recordClear(realCtx, view.get());
            m_ore.endFrame(renderContext);
            ore_gm::invalidateGLStateAfterOre(renderContext);
            drawCanvasToScreen(originalRenderer);
        }
#endif
    }

private:
    bool m_deferred;
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(render_deferred_canvas_immediate,
           return new RenderDeferredCanvasGM(false))
GMREGISTER(render_deferred_canvas, return new RenderDeferredCanvasGM(true))
