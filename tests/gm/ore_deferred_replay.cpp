/*
 * Copyright 2026 Rive
 *
 * Renders one triangle immediately, via record and replay, and via the
 * context's inline deferred flag. The goldens must be byte identical.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"
#if ORE_GM_HAS_BACKEND
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"
#include "rive/renderer/ore/cmd/ore_render_pass_recording.hpp"
#include "rive/renderer/ore/cmd/ore_replay.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_render_pass.hpp"
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;
#if ORE_GM_HAS_BACKEND
using namespace rive::ore;
// Disambiguates from rive::cmd.
#endif

// kInlineDeferred drives the context deferred flag through the same chooser
// the Lua beginRenderPass call site uses.
enum class ReplayMode
{
    kImmediate,
    kRecordReplay,
    kInlineDeferred,
};

class OreDeferredReplayGM : public GM
{
public:
    OreDeferredReplayGM(ReplayMode mode) : GM(256, 256), m_mode(mode) {}

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
        bd.label = "ore_deferred_replay_vb";
        auto vb = ctx.makeBuffer(bd);
        if (!vb)
            return;

        auto shader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
        if (!shader.vsModule)
            return;

        ore_gm::TrianglePipeline tri(shader,
                                     colorTarget->texture()->format(),
                                     "ore_deferred_replay_pipeline");
        auto pipeline = ctx.makePipeline(tri.desc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_deferred_replay] pipeline failed: %s\n",
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
        rpDesc.label = "ore_deferred_replay_pass";

        m_ore.beginFrame(renderContext);

        if (m_mode == ReplayMode::kRecordReplay)
        {
            // Same calls as the immediate branch below.
            ore::cmd::OreCommandBuffer cmdBuf;
            {
                ore::cmd::RenderPassRecording rec(&ctx, &cmdBuf, rpDesc);
                rec.setPipeline(pipeline.get());
                rec.setVertexBuffer(0, vb.get());
                rec.setViewport(0, 0, 256, 256);
                rec.draw(3);
                rec.finish();
            }
            ore::cmd::replayCommandBuffer(ctx, cmdBuf);
        }
        else if (m_mode == ReplayMode::kInlineDeferred)
        {
            // finish records and then inline replays.
            ctx.setDeferredRecording(true);
            auto pass =
                ore::cmd::beginRenderPassRecordingOrImmediate(ctx, rpDesc);
            pass->setPipeline(pipeline.get());
            pass->setVertexBuffer(0, vb.get());
            pass->setViewport(0, 0, 256, 256);
            pass->draw(3);
            pass->finish();
            ctx.setDeferredRecording(false);
        }
        else
        {
            auto pass = ctx.beginRenderPass(rpDesc);
            pass->setPipeline(pipeline.get());
            pass->setVertexBuffer(0, vb.get());
            pass->setViewport(0, 0, 256, 256);
            pass->draw(3);
            pass->finish();
        }

        m_ore.endFrame(renderContext);
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
    ReplayMode m_mode;
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_deferred_replay_immediate,
           return new OreDeferredReplayGM(ReplayMode::kImmediate))
GMREGISTER(ore_deferred_replay,
           return new OreDeferredReplayGM(ReplayMode::kRecordReplay))
GMREGISTER(ore_deferred_replay_inline,
           return new OreDeferredReplayGM(ReplayMode::kInlineDeferred))
