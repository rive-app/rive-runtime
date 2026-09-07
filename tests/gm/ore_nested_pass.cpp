/*
 * Copyright 2026 Rive
 *
 * A pass begun inside another. Canvas B's pass opens first, the pass that
 * draws the triangle into canvas A begins and finishes inside it, then B
 * samples A. The nested pass has to run ahead of B for B to see this frame's
 * triangle, so every variant must match drawing A then B in order.
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
#include "rive/renderer/ore/cmd/ore_deferred_render_pass.hpp"
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

enum class NestMode
{
    kImmediate, // A then B, the reference
    kInline,    // nested through the script entry point on the live context
    kRecorded,  // nested into one recording, then replayed
};

class OreNestedPassGM : public GM
{
public:
    OreNestedPassGM(NestMode mode) : GM(256, 256), m_mode(mode) {}

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

        BufferDesc bd{};
        bd.usage = BufferUsage::vertex;
        bd.size = sizeof(ore_gm::kTriVertices);
        bd.data = ore_gm::kTriVertices;
        bd.label = "ore_nested_pass_vb";
        auto vb = ctx.makeBuffer(bd);
        if (!vb)
            return;

        auto triShader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
        if (!triShader.vsModule)
            return;

        ore_gm::TrianglePipeline tri(triShader,
                                     targetA->texture()->format(),
                                     "ore_nested_pass_tri");
        auto triPipeline = ctx.makePipeline(tri.desc);
        if (!triPipeline)
        {
            fprintf(stderr,
                    "[ore_nested_pass] tri pipeline failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

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
        imgPd.label = "ore_nested_pass_img";
        auto imgPipeline = ctx.makePipeline(imgPd);
        if (!imgPipeline)
        {
            fprintf(stderr,
                    "[ore_nested_pass] img pipeline failed: %s\n",
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
        rpA.label = "ore_nested_pass_passA";

        ColorAttachment caB{};
        caB.view = targetB.get();
        caB.loadOp = LoadOp::clear;
        caB.storeOp = StoreOp::store;
        caB.clearColor = {0, 0, 0, 1};
        RenderPassDesc rpB{};
        rpB.colorAttachments[0] = caB;
        rpB.colorCount = 1;
        rpB.label = "ore_nested_pass_passB";

        auto drawA = [&](RenderPass& pass) {
            pass.setPipeline(triPipeline.get());
            pass.setVertexBuffer(0, vb.get());
            pass.setViewport(0, 0, 256, 256);
            pass.draw(3);
            pass.finish();
        };
        auto sampleA = [&](RenderPass& pass) {
            pass.setBindGroup(1, texBG.get());
            pass.setBindGroup(2, sampBG.get());
            pass.draw(6);
            pass.finish();
        };

        m_ore.beginFrame(renderContext);

        if (m_mode == NestMode::kImmediate)
        {
            drawA(*ctx.beginRenderPass(rpA));
            auto pB = ctx.beginRenderPass(rpB);
            pB->setPipeline(imgPipeline.get());
            pB->setViewport(0, 0, 256, 256);
            sampleA(*pB);
        }
        else if (m_mode == NestMode::kInline)
        {
            auto pB = ore::cmd::beginRecordedRenderPass(ctx, rpB);
            pB->setPipeline(imgPipeline.get());
            pB->setViewport(0, 0, 256, 256);
            drawA(*ore::cmd::beginRecordedRenderPass(ctx, rpA));
            sampleA(*pB);
        }
        else
        {
            ore::cmd::OreCommandBuffer cmdBuf;
            {
                ore::cmd::RenderPassRecording pB(&ctx, &cmdBuf, rpB);
                pB.setPipeline(imgPipeline.get());
                pB.setViewport(0, 0, 256, 256);
                ore::cmd::RenderPassRecording pA(&ctx, &cmdBuf, rpA);
                drawA(pA);
                sampleA(pB);
            }
            ore::cmd::replayCommandBuffer(ctx, cmdBuf);
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
    NestMode m_mode;
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_nested_pass_immediate,
           return new OreNestedPassGM(NestMode::kImmediate))
GMREGISTER(ore_nested_pass, return new OreNestedPassGM(NestMode::kInline))
GMREGISTER(ore_nested_pass_recorded,
           return new OreNestedPassGM(NestMode::kRecorded))
