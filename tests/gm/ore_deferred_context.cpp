/*
 * Copyright 2026 Rive
 *
 * Builds the same triangle through the same API on the real ore Context and on
 * a DeferredOreContext whose replay creates the real resources. The goldens
 * must be byte identical.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"
#if ORE_GM_HAS_BACKEND
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;
#if ORE_GM_HAS_BACKEND
using namespace rive::ore;
// Disambiguates from rive::cmd.
#endif

class OreDeferredContextGM : public GM
{
public:
    OreDeferredContextGM(bool deferred) : GM(256, 256), m_deferred(deferred) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#if ORE_GM_HAS_BACKEND
        auto& realCtx = *renderContext->getOreContext();
        auto canvas = renderContext->makeRenderCanvas(256, 256);
        if (!canvas)
            return;

        // Same call sites for both modes, only ctx differs. The deferred
        // context delegates wrapCanvasTexture to the real one.
        ore::cmd::DeferredOreContext dctx(&realCtx);
        Context& ctx = m_deferred ? static_cast<Context&>(dctx) : realCtx;

        auto colorTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!colorTarget)
            return;

        auto shader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
        if (!shader.vsModule)
            return;

        BufferDesc bd{};
        bd.usage = BufferUsage::vertex;
        bd.size = sizeof(ore_gm::kTriVertices);
        bd.data = ore_gm::kTriVertices;
        bd.label = "ore_deferred_context_vb";
        auto vb = ctx.makeBuffer(bd);
        if (!vb)
            return;

        ore_gm::TrianglePipeline tri(shader,
                                     colorTarget->texture()->format(),
                                     "ore_deferred_context_pipeline");
        auto pipeline = ctx.makePipeline(tri.desc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_deferred_context] pipeline failed: %s\n",
                    realCtx.lastError().c_str());
            return;
        }

        ColorAttachment ca{};
        ca.view = colorTarget.get();
        ca.loadOp = LoadOp::clear;
        ca.storeOp = StoreOp::store;
        ca.clearColor = {0.1f, 0.1f, 0.15f, 1.0f};
        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_deferred_context_pass";

        auto issuePass = [&](Context& c) {
            auto pass = c.beginRenderPass(rpDesc);
            pass->setPipeline(pipeline.get());
            pass->setVertexBuffer(0, vb.get());
            pass->setViewport(0, 0, 256, 256);
            pass->draw(3);
            pass->finish();
        };

        if (m_deferred)
        {
            issuePass(dctx);
            m_ore.beginFrame(renderContext);
            dctx.replay(realCtx);
            m_ore.endFrame(renderContext);
        }
        else
        {
            m_ore.beginFrame(renderContext);
            issuePass(realCtx); // immediate GPU work needs an active frame
            m_ore.endFrame(renderContext);
        }

        ore_gm::invalidateGLStateAfterOre(renderContext);

        originalRenderer->save();
        originalRenderer->drawImage(canvas->renderImage(),
                                    {.filter = ImageFilter::nearest},
                                    BlendMode::srcOver,
                                    1);
        originalRenderer->restore();
#endif
    }

private:
    bool m_deferred;
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_deferred_context_immediate,
           return new OreDeferredContextGM(false))
GMREGISTER(ore_deferred_context, return new OreDeferredContextGM(true))
