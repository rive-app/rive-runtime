/*
 * Copyright 2026 Rive
 *
 * Renders a triangle into canvas A then samples A into canvas B, immediately
 * and via one recorded command buffer. Sequential replay must preserve the
 * pass dependency. The goldens must be byte identical.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"
#if ORE_GM_HAS_BACKEND
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"
#include "rive/renderer/ore/cmd/ore_render_pass_recording.hpp"
#include "rive/renderer/ore/cmd/ore_replay.hpp"
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;
#if ORE_GM_HAS_BACKEND
using namespace rive::ore;
// Disambiguates from rive::cmd.
#endif

enum class MPMode
{
    kImmediate,
    kDeferred,
};

class OreDeferredMultipassGM : public GM
{
public:
    OreDeferredMultipassGM(MPMode mode) : GM(256, 256), m_mode(mode) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#if ORE_GM_HAS_BACKEND
        auto& ctx = *renderContext->getOreContext();

        // Canvas A is the producer and B the consumer.
        auto canvasA = renderContext->makeRenderCanvas(256, 256);
        auto canvasB = renderContext->makeRenderCanvas(256, 256);
        if (!canvasA || !canvasB)
            return;
        auto targetA = ctx.wrapCanvasTexture(canvasA.get());
        auto targetB = ctx.wrapCanvasTexture(canvasB.get());
        if (!targetA || !targetB)
            return;

        // Pass 1 resources.
        BufferDesc bd{};
        bd.usage = BufferUsage::vertex;
        bd.size = sizeof(ore_gm::kTriVertices);
        bd.data = ore_gm::kTriVertices;
        bd.label = "ore_deferred_multipass_vb";
        auto vb = ctx.makeBuffer(bd);
        if (!vb)
            return;

        auto triShader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
        if (!triShader.vsModule)
            return;

        ore_gm::TrianglePipeline tri(triShader,
                                     targetA->texture()->format(),
                                     "ore_deferred_multipass_tri");
        auto triPipeline = ctx.makePipeline(tri.desc);
        if (!triPipeline)
        {
            fprintf(stderr,
                    "[ore_deferred_multipass] tri pipeline failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // Pass 2 resources.
        SamplerDesc sampDesc{};
        sampDesc.minFilter = Filter::nearest;
        sampDesc.magFilter = Filter::nearest;
        auto sampler = ctx.makeSampler(sampDesc);

        auto imgShader = ore_gm::loadShader(ctx, ore_gm::kImageView);
        if (!imgShader.vsModule)
            return;
        auto layout1 =
            ore_gm::makeLayoutFromShader(ctx, imgShader.vsModule.get(), 1);
        auto layout2 =
            ore_gm::makeLayoutFromShader(ctx, imgShader.vsModule.get(), 2);
        BindGroupLayout* layouts[] = {nullptr, layout1.get(), layout2.get()};

        PipelineDesc imgPd{};
        imgPd.vertexModule = imgShader.vsModule.get();
        imgPd.fragmentModule = imgShader.psModule.get();
        imgPd.vertexEntryPoint = imgShader.vsEntryPoint;
        imgPd.fragmentEntryPoint = imgShader.fsEntryPoint;
        imgPd.vertexBufferCount = 0;
        imgPd.topology = PrimitiveTopology::triangleList;
        imgPd.colorTargets[0].format = targetB->texture()->format();
        imgPd.colorCount = 1;
        imgPd.depthStencil.depthCompare = CompareFunction::always;
        imgPd.depthStencil.depthWriteEnabled = false;
        imgPd.bindGroupLayouts = layouts;
        imgPd.bindGroupLayoutCount = 3;
        imgPd.label = "ore_deferred_multipass_img";
        auto imgPipeline = ctx.makePipeline(imgPd);
        if (!imgPipeline)
        {
            fprintf(stderr,
                    "[ore_deferred_multipass] img pipeline failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        BindGroupDesc texBGDesc{};
        texBGDesc.layout = layout1.get();
        BindGroupDesc::TexEntry texEntry{};
        texEntry.slot = 0;
        texEntry.view = targetA.get();
        texBGDesc.textures = &texEntry;
        texBGDesc.textureCount = 1;
        auto texBG = ctx.makeBindGroup(texBGDesc);

        BindGroupDesc sampBGDesc{};
        sampBGDesc.layout = layout2.get();
        BindGroupDesc::SampEntry sampEntry{};
        sampEntry.slot = 0;
        sampEntry.sampler = sampler.get();
        sampBGDesc.samplers = &sampEntry;
        sampBGDesc.samplerCount = 1;
        auto sampBG = ctx.makeBindGroup(sampBGDesc);

        ColorAttachment caA{};
        caA.view = targetA.get();
        caA.loadOp = LoadOp::clear;
        caA.storeOp = StoreOp::store;
        caA.clearColor = {0.1f, 0.1f, 0.15f, 1.0f};
        RenderPassDesc rpA{};
        rpA.colorAttachments[0] = caA;
        rpA.colorCount = 1;
        rpA.label = "ore_deferred_multipass_passA";

        ColorAttachment caB{};
        caB.view = targetB.get();
        caB.loadOp = LoadOp::clear;
        caB.storeOp = StoreOp::store;
        caB.clearColor = {0, 0, 0, 1};
        RenderPassDesc rpB{};
        rpB.colorAttachments[0] = caB;
        rpB.colorCount = 1;
        rpB.label = "ore_deferred_multipass_passB";

        m_ore.beginFrame(renderContext);

        if (m_mode == MPMode::kDeferred)
        {
            ore::cmd::OreCommandBuffer cmdBuf;
            {
                ore::cmd::RenderPassRecording p1(&ctx, &cmdBuf, rpA);
                p1.setPipeline(triPipeline.get());
                p1.setVertexBuffer(0, vb.get());
                p1.setViewport(0, 0, 256, 256);
                p1.draw(3);
                p1.finish();

                ore::cmd::RenderPassRecording p2(&ctx, &cmdBuf, rpB);
                p2.setPipeline(imgPipeline.get());
                p2.setBindGroup(1, texBG.get());
                p2.setBindGroup(2, sampBG.get());
                p2.setViewport(0, 0, 256, 256);
                p2.draw(6);
                p2.finish();
            }
            ore::cmd::replayCommandBuffer(ctx, cmdBuf);
        }
        else
        {
            auto p1 = ctx.beginRenderPass(rpA);
            p1->setPipeline(triPipeline.get());
            p1->setVertexBuffer(0, vb.get());
            p1->setViewport(0, 0, 256, 256);
            p1->draw(3);
            p1->finish();

            auto p2 = ctx.beginRenderPass(rpB);
            p2->setPipeline(imgPipeline.get());
            p2->setBindGroup(1, texBG.get());
            p2->setBindGroup(2, sampBG.get());
            p2->setViewport(0, 0, 256, 256);
            p2->draw(6);
            p2->finish();
        }

        m_ore.endFrame(renderContext);
        ore_gm::invalidateGLStateAfterOre(renderContext);

        originalRenderer->save();
        originalRenderer->drawImage(canvasB->renderImage(),
                                    {.filter = ImageFilter::nearest},
                                    BlendMode::srcOver,
                                    1);
        originalRenderer->restore();
#endif
    }

private:
    MPMode m_mode;
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_deferred_multipass_immediate,
           return new OreDeferredMultipassGM(MPMode::kImmediate))
GMREGISTER(ore_deferred_multipass,
           return new OreDeferredMultipassGM(MPMode::kDeferred))
