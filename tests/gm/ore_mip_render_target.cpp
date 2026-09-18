/*
 * Copyright 2026 Rive
 *
 * Renders into every mip level of one texture through a per level view, with
 * colour and depth attached and no explicit viewport, then shows each level
 * in its own quadrant. A pass sized from the texture instead of the attached
 * level draws the triangle scaled and shifted in the smaller quadrants. Level
 * one is then loaded back and gets a second, flipped triangle. Each quadrant
 * picks its level with a sampler lod clamp, since GL cannot sample a mip view.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"
#if ORE_GM_HAS_BACKEND
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;
#if ORE_GM_HAS_BACKEND
using namespace rive::ore;
#endif

class OreMipRenderTargetGM : public GM
{
public:
    OreMipRenderTargetGM() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#if ORE_GM_HAS_BACKEND
        auto& ctx = *renderContext->getOreContext();
        constexpr uint32_t kSize = 64;
        constexpr uint32_t kLevels = 4;

        TextureDesc colorDesc{};
        colorDesc.width = kSize;
        colorDesc.height = kSize;
        colorDesc.format = TextureFormat::rgba8unorm;
        colorDesc.renderTarget = true;
        colorDesc.numMipmaps = kLevels;
        colorDesc.label = "ore_mip_render_target_color";
        auto colorTex = ctx.makeTexture(colorDesc);
        TextureDesc depthDesc = colorDesc;
        depthDesc.format = TextureFormat::depth32float;
        depthDesc.label = "ore_mip_render_target_depth";
        auto depthTex = ctx.makeTexture(depthDesc);
        if (!colorTex || !depthTex)
            return;

        rcp<TextureView> colorViews[kLevels];
        rcp<TextureView> depthViews[kLevels];
        for (uint32_t i = 0; i < kLevels; ++i)
        {
            TextureViewDesc vd{};
            vd.texture = colorTex.get();
            vd.dimension = TextureViewDimension::texture2D;
            vd.baseMipLevel = i;
            vd.mipCount = 1;
            vd.baseLayer = 0;
            vd.layerCount = 1;
            colorViews[i] = ctx.makeTextureView(vd);
            vd.texture = depthTex.get();
            depthViews[i] = ctx.makeTextureView(vd);
            if (!colorViews[i] || !depthViews[i])
                return;
        }

        BufferDesc bd{};
        bd.usage = BufferUsage::vertex;
        bd.size = sizeof(ore_gm::kTriVertices);
        bd.data = ore_gm::kTriVertices;
        bd.label = "ore_mip_render_target_vb";
        auto vb = ctx.makeBuffer(bd);
        ore_gm::TriVertex flipped[3];
        for (int i = 0; i < 3; ++i)
        {
            flipped[i] = ore_gm::kTriVertices[i];
            flipped[i].y = -flipped[i].y;
        }
        bd.data = flipped;
        bd.label = "ore_mip_render_target_vb_flipped";
        auto vbFlipped = ctx.makeBuffer(bd);
        if (!vb || !vbFlipped)
            return;

        auto triShader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
        if (!triShader.vsModule)
            return;
        ore_gm::TrianglePipeline tri(triShader,
                                     TextureFormat::rgba8unorm,
                                     "ore_mip_render_target_tri");
        tri.desc.depthStencil.format = TextureFormat::depth32float;
        tri.desc.depthStencil.depthCompare = CompareFunction::always;
        tri.desc.depthStencil.depthWriteEnabled = true;
        auto triPipeline = ctx.makePipeline(tri.desc);
        if (!triPipeline)
        {
            fprintf(stderr,
                    "[ore_mip_render_target] tri pipeline failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        auto canvas = renderContext->makeRenderCanvas(256, 256);
        if (!canvas)
            return;
        auto canvasTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!canvasTarget)
            return;

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
        imgPd.colorTargets[0].format = canvasTarget->texture()->format();
        imgPd.colorCount = 1;
        imgPd.depthStencil.depthCompare = CompareFunction::always;
        imgPd.depthStencil.depthWriteEnabled = false;
        imgPd.bindGroupLayouts = layouts;
        imgPd.bindGroupLayoutCount = 3;
        imgPd.label = "ore_mip_render_target_img";
        auto imgPipeline = ctx.makePipeline(imgPd);
        if (!imgPipeline)
        {
            fprintf(stderr,
                    "[ore_mip_render_target] img pipeline failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        TextureViewDesc chainDesc{};
        chainDesc.texture = colorTex.get();
        chainDesc.dimension = TextureViewDimension::texture2D;
        chainDesc.baseMipLevel = 0;
        chainDesc.mipCount = kLevels;
        chainDesc.baseLayer = 0;
        chainDesc.layerCount = 1;
        auto chainView = ctx.makeTextureView(chainDesc);
        if (!chainView)
            return;
        BindGroupDesc texBGDesc{};
        texBGDesc.layout = layout1.get();
        BindGroupDesc::TexEntry texEntry{};
        texEntry.slot = 0;
        texEntry.view = chainView.get();
        texBGDesc.textures = &texEntry;
        texBGDesc.textureCount = 1;
        auto texBG = ctx.makeBindGroup(texBGDesc);

        rcp<Sampler> samplers[kLevels];
        rcp<BindGroup> sampBGs[kLevels];
        for (uint32_t i = 0; i < kLevels; ++i)
        {
            SamplerDesc sampDesc{};
            sampDesc.minFilter = Filter::nearest;
            sampDesc.magFilter = Filter::nearest;
            sampDesc.mipmapFilter = Filter::nearest;
            sampDesc.minLod = static_cast<float>(i);
            sampDesc.maxLod = static_cast<float>(i);
            samplers[i] = ctx.makeSampler(sampDesc);
            BindGroupDesc sampBGDesc{};
            sampBGDesc.layout = layout2.get();
            BindGroupDesc::SampEntry sampEntry{};
            sampEntry.slot = 0;
            sampEntry.sampler = samplers[i].get();
            sampBGDesc.samplers = &sampEntry;
            sampBGDesc.samplerCount = 1;
            sampBGs[i] = ctx.makeBindGroup(sampBGDesc);
        }

        m_ore.beginFrame(renderContext);

        static constexpr float kClears[kLevels][4] = {
            {0.10f, 0.10f, 0.15f, 1.0f},
            {0.35f, 0.10f, 0.10f, 1.0f},
            {0.10f, 0.35f, 0.10f, 1.0f},
            {0.10f, 0.10f, 0.35f, 1.0f},
        };
        for (uint32_t i = 0; i < kLevels; ++i)
        {
            RenderPassDesc rp{};
            rp.colorAttachments[0].view = colorViews[i].get();
            rp.colorAttachments[0].loadOp = LoadOp::clear;
            rp.colorAttachments[0].storeOp = StoreOp::store;
            rp.colorAttachments[0].clearColor = {kClears[i][0],
                                                 kClears[i][1],
                                                 kClears[i][2],
                                                 kClears[i][3]};
            rp.colorCount = 1;
            rp.depthStencil.view = depthViews[i].get();
            rp.depthStencil.depthLoadOp = LoadOp::clear;
            rp.depthStencil.depthStoreOp = StoreOp::store;
            rp.label = "ore_mip_render_target_level";
            auto pass = ctx.beginRenderPass(rp);
            pass->setPipeline(triPipeline.get());
            pass->setVertexBuffer(0, vb.get());
            pass->draw(3);
            pass->finish();
        }

        {
            RenderPassDesc rp{};
            rp.colorAttachments[0].view = colorViews[1].get();
            rp.colorAttachments[0].loadOp = LoadOp::load;
            rp.colorAttachments[0].storeOp = StoreOp::store;
            rp.colorCount = 1;
            rp.depthStencil.view = depthViews[1].get();
            rp.depthStencil.depthLoadOp = LoadOp::load;
            rp.depthStencil.depthStoreOp = StoreOp::store;
            rp.label = "ore_mip_render_target_reload";
            auto pass = ctx.beginRenderPass(rp);
            pass->setPipeline(triPipeline.get());
            pass->setVertexBuffer(0, vbFlipped.get());
            pass->draw(3);
            pass->finish();
        }

        RenderPassDesc rpShow{};
        rpShow.colorAttachments[0].view = canvasTarget.get();
        rpShow.colorAttachments[0].loadOp = LoadOp::clear;
        rpShow.colorAttachments[0].storeOp = StoreOp::store;
        rpShow.colorAttachments[0].clearColor = {0, 0, 0, 1};
        rpShow.colorCount = 1;
        rpShow.label = "ore_mip_render_target_show";
        auto show = ctx.beginRenderPass(rpShow);
        show->setPipeline(imgPipeline.get());
        show->setBindGroup(1, texBG.get());
        for (uint32_t i = 0; i < kLevels; ++i)
        {
            show->setViewport((i % 2) * 128.0f, (i / 2) * 128.0f, 128, 128);
            show->setBindGroup(2, sampBGs[i].get());
            show->draw(6);
        }
        show->finish();

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
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_mip_render_target, return new OreMipRenderTargetGM)
