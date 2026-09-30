/*
 * Copyright 2026 Rive
 *
 * Ore clears Rive's own render target and draws a triangle into it, then Rive
 * draws a path on top, immediately and through a DeferredSession whose target
 * view is wrapped at replay. The goldens must be byte identical and show both.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_sink.hpp"
#if ORE_GM_HAS_BACKEND
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;

class OreDeferredTargetGM : public GM
{
public:
    OreDeferredTargetGM(bool deferred) : GM(256, 256), m_deferred(deferred) {}

    // The first Ore pass covers the target, so Rive must not clear over it.
    void updateFrameOptions(TestingWindow::FrameOptions* options) const override
    {
        options->doClear = false;
    }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        TestingWindow* window = TestingWindow::Get();
        bool exposed = renderContext != nullptr &&
                       m_ore.ensureContext(renderContext) &&
                       window->oreTarget().exposed;
#if ORE_GM_HAS_BACKEND
        if (exposed)
        {
            if (m_deferred)
            {
                drawDeferred(renderContext, originalRenderer);
            }
            else
            {
                drawImmediate(renderContext, originalRenderer);
            }
            return;
        }
#endif
        // Keep the frame defined where no target is exposed.
        drawBackground(*window->factory(), originalRenderer);
        drawPath(*window->factory(), originalRenderer);
    }

private:
    static void drawBackground(Factory& factory, Renderer* renderer)
    {
        auto paint = factory.makeRenderPaint();
        paint->color(0xff1ab38c);
        // Overshoots the frame since Rive loads it, and a rasterizer that
        // leaves the edge short of full coverage would blend the last frame.
        auto path = factory.makeEmptyRenderPath();
        path->moveTo(-1, -1);
        path->lineTo(257, -1);
        path->lineTo(257, 257);
        path->lineTo(-1, 257);
        path->close();
        renderer->drawPath(path.get(), paint.get());
    }

    static void drawPath(Factory& factory, Renderer* renderer)
    {
        auto paint = factory.makeRenderPaint();
        paint->color(0xc0e02040);
        auto path = factory.makeEmptyRenderPath();
        path->moveTo(128, 96);
        path->lineTo(224, 160);
        path->lineTo(128, 224);
        path->lineTo(32, 160);
        path->close();
        renderer->drawPath(path.get(), paint.get());
    }

#if ORE_GM_HAS_BACKEND
    // Clears the whole target, as the contract asks of the first pass.
    void recordTriangle(ore::Context& ctx, ore::TextureView* target)
    {
        auto shader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
        if (!shader.vsModule)
        {
            return;
        }
        ore::BufferDesc bd{};
        bd.usage = ore::BufferUsage::vertex;
        bd.size = sizeof(ore_gm::kTriVertices);
        bd.data = ore_gm::kTriVertices;
        bd.label = "ore_deferred_target_vb";
        auto vb = ctx.makeBuffer(bd);
        ore_gm::TrianglePipeline tri(shader,
                                     target->texture()->format(),
                                     "ore_deferred_target_pipeline");
        auto pipeline = ctx.makePipeline(tri.desc);
        if (!vb || !pipeline)
        {
            return;
        }

        ore::ColorAttachment ca{};
        ca.view = target;
        ca.loadOp = ore::LoadOp::clear;
        ca.storeOp = ore::StoreOp::store;
        ca.clearColor = {0.10f, 0.70f, 0.55f, 1.0f};
        ore::RenderPassDesc rp{};
        rp.colorAttachments[0] = ca;
        rp.colorCount = 1;
        rp.label = "ore_deferred_target_pass";
        auto pass = ctx.beginRenderPass(rp);
        pass->setPipeline(pipeline.get());
        pass->setVertexBuffer(0, vb.get());
        pass->setViewport(0, 0, 256, 256);
        pass->draw(3);
        pass->finish();
    }

    void drawImmediate(RenderContext* renderContext, Renderer* renderer)
    {
        auto& realCtx = *renderContext->getOreContext();
        m_ore.beginFrame(renderContext);
        auto target =
            realCtx.wrapRenderTarget(TestingWindow::Get()->renderTarget());
        if (target != nullptr)
        {
            recordTriangle(realCtx, target.get());
        }
        m_ore.endFrame(renderContext);
        ore_gm::invalidateGLStateAfterOre(renderContext);
        Factory& factory = *TestingWindow::Get()->factory();
        if (target == nullptr)
        {
            // Rive loads the target, so an unwrapped one must still be covered.
            drawBackground(factory, renderer);
        }
        drawPath(factory, renderer);
    }

    void drawDeferred(RenderContext* renderContext, Renderer* renderer)
    {
        auto& realCtx = *renderContext->getOreContext();
        TestingWindow* window = TestingWindow::Get();
        rive::cmd::DeferredSession session(ore::ReplayCaps::from(realCtx));
        session.oreContext().setTarget(
            ore::Context::TargetDesc::color8(window->width(),
                                             window->height(),
                                             window->oreTarget().bgra));
        auto target = session.oreContext().targetView();
        recordTriangle(session.oreContext(), target.get());
        session.recordOreReplayMarker();

        auto screen = session.makeScreenRenderer();
        drawPath(session, screen.get());

        rive::cmd::DeferredFrame frame = rive::cmd::snapshotFrame(session);
        ore_gm::GMFrameSink sink(renderContext,
                                 renderer,
                                 &m_ore,
                                 window->renderTarget());
        rive::cmd::DeferredReplayer replayer;
        replayer.replayFrame(frame, sink);
    }
#endif

    bool m_deferred;
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_deferred_target_immediate, return new OreDeferredTargetGM(false))
GMREGISTER(ore_deferred_target, return new OreDeferredTargetGM(true))
