/*
 * Copyright 2026 Rive
 *
 * Renders the same triangle from an immediate vertex buffer, a replay created
 * buffer, and a deferred buffer remapped at unified replay, all through the
 * DeferredOreContext. The goldens must be byte identical.
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
#include <memory>
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;
#if ORE_GM_HAS_BACKEND
using namespace rive::ore;
// Disambiguates from rive::cmd.
#endif

// kReplayBuffer creates the buffer via replay then draws immediately.
// kUnified records the whole pass against a deferred buffer and a single
// replay creates the real buffer and remaps it.
enum class ResMode
{
    kImmediate,
    kReplayBuffer,
    kUnified,
};

class OreDeferredResourceGM : public GM
{
public:
    OreDeferredResourceGM(ResMode mode) : GM(256, 256), m_mode(mode) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#if ORE_GM_HAS_BACKEND
        auto& ctx = *renderContext->getOreContext();
        auto canvas = renderContext->makeRenderCanvas(256, 256);
        if (!canvas)
            return;
        auto colorTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!colorTarget)
            return;

        BufferDesc bd{};
        bd.usage = BufferUsage::vertex;
        bd.size = sizeof(ore_gm::kTriVertices);
        bd.data = ore_gm::kTriVertices;
        bd.label = "ore_deferred_resource_vb";

        auto shader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
        if (!shader.vsModule)
            return;

        ore_gm::TrianglePipeline tri(shader,
                                     colorTarget->texture()->format(),
                                     "ore_deferred_resource_pipeline");
        auto pipeline = ctx.makePipeline(tri.desc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_deferred_resource] pipeline failed: %s\n",
                    ctx.lastError().c_str());
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
        rpDesc.label = "ore_deferred_resource_pass";

        if (m_mode == ResMode::kUnified)
        {
            // The pass is recorded before any real buffer exists; the real
            // pipeline and target resolve by flagged index at replay.
            ore::cmd::DeferredOreContext dctx(&ctx);
            auto vb = dctx.makeBuffer(bd);
            {
                auto pass = dctx.beginRenderPass(rpDesc);
                pass->setPipeline(pipeline.get());
                pass->setVertexBuffer(0, vb.get());
                pass->setViewport(0, 0, 256, 256);
                pass->draw(3);
                pass->finish();
            }

            m_ore.beginFrame(renderContext);
            dctx.replay(ctx);
            m_ore.endFrame(renderContext);
        }
        else
        {
            rcp<Buffer> vb;
            ore::cmd::OreResident table;
            std::unique_ptr<ore::cmd::DeferredOreContext> dctx;
            if (m_mode == ResMode::kReplayBuffer)
            {
                dctx = std::make_unique<ore::cmd::DeferredOreContext>(&ctx);
                auto deferredVb = dctx->makeBuffer(bd);
                dctx->replayFrame(ctx, table);
                auto* real = table.get(
                    static_cast<ore::cmd::DeferredBuffer*>(deferredVb.get())
                        ->clientHandle());
                vb = ref_rcp(static_cast<Buffer*>(real));
            }
            else
            {
                vb = ctx.makeBuffer(bd);
            }
            if (!vb)
                return;

            m_ore.beginFrame(renderContext);
            auto pass = ctx.beginRenderPass(rpDesc);
            pass->setPipeline(pipeline.get());
            pass->setVertexBuffer(0, vb.get());
            pass->setViewport(0, 0, 256, 256);
            pass->draw(3);
            pass->finish();
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
    ResMode m_mode;
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_deferred_resource_immediate,
           return new OreDeferredResourceGM(ResMode::kImmediate))
GMREGISTER(ore_deferred_resource,
           return new OreDeferredResourceGM(ResMode::kReplayBuffer))
GMREGISTER(ore_deferred_resource_unified,
           return new OreDeferredResourceGM(ResMode::kUnified))
