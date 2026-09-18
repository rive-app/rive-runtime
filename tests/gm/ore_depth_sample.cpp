/*
 * Copyright 2026 Rive
 *
 * ── Non-comparison depth-sample witness ──
 *
 * Clears a `depth32float` target to 1.0 in a depth-only pass, then samples
 * it through a REGULAR sampler with both `textureSampleLevel` (explicit
 * lod) and `textureSample` (implicit lod) — the depth-prepass read shape
 * (a G-buffer depth reconstruction, a fog raymarch start distance).
 *
 * This is distinct from `ore_binding_shadow_sampler*`, which use
 * `textureSampleCompare` through a `sampler_comparison`. The
 * non-comparison read used to break the HLSL SM5.0 bake: SPIRV-Cross
 * emitted `SampleCmp` / `SampleCmpLevelZero` for ANY sample of a depth
 * image, and FXC rejects those against a `SamplerState` (error X3013), so
 * `makeShaderModule` failed on D3D11/D3D12 while every naga-fed backend
 * rendered fine. Fixed by `spirv_cross_patches/0001` (only a Dref operand
 * selects the Cmp forms).
 *
 * ── Expected output ──
 *
 *   Solid green — tint(0,1,0,1) × depth(1.0) × depth(1.0).
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_VK) || defined(ORE_BACKEND_WGPU) ||                    \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12) ||                \
    defined(ORE_BACKEND_RHI)
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_VK) || defined(ORE_BACKEND_WGPU) ||                    \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12) ||                \
    defined(ORE_BACKEND_RHI)
using namespace rive::ore;
#define ORE_DEPTH_SAMPLE_ACTIVE
#endif

class OreDepthSampleGM : public GM
{
public:
    OreDepthSampleGM() : GM(128, 128) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_DEPTH_SAMPLE_ACTIVE
        auto& ctx = *renderContext->getOreContext();

        struct Uniforms
        {
            float tint[4];
        };
        static const Uniforms kU = {{0.0f, 1.0f, 0.0f, 1.0f}};
        BufferDesc uboDesc{};
        uboDesc.usage = BufferUsage::uniform;
        uboDesc.size = sizeof(Uniforms);
        uboDesc.data = &kU;
        uboDesc.label = "depth_sample_ubo";
        auto uboBuf = ctx.makeBuffer(uboDesc);

        TextureDesc depthDesc{};
        depthDesc.width = 1;
        depthDesc.height = 1;
        depthDesc.format = TextureFormat::depth32float;
        depthDesc.type = TextureType::texture2D;
        depthDesc.renderTarget = true;
        depthDesc.numMipmaps = 1;
        depthDesc.sampleCount = 1;
        depthDesc.label = "depth_sample_depth";
        auto depthTex = ctx.makeTexture(depthDesc);
        if (!depthTex)
        {
            fprintf(stderr,
                    "[ore_depth_sample] depthTex makeTexture failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        TextureViewDesc depthViewDesc{};
        depthViewDesc.texture = depthTex.get();
        depthViewDesc.dimension = TextureViewDimension::texture2D;
        depthViewDesc.baseMipLevel = 0;
        depthViewDesc.mipCount = 1;
        depthViewDesc.baseLayer = 0;
        depthViewDesc.layerCount = 1;
        auto depthView = ctx.makeTextureView(depthViewDesc);
        if (!depthView)
        {
            fprintf(stderr,
                    "[ore_depth_sample] depthView makeTextureView failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        SamplerDesc sampDesc{};
        sampDesc.minFilter = Filter::nearest;
        sampDesc.magFilter = Filter::nearest;
        sampDesc.label = "depth_sample_point";
        auto pointSampler = ctx.makeSampler(sampDesc);

        auto shader = ore_gm::loadShader(ctx, ore_gm::kDepthSampleWitness);
        if (!shader.vsModule || !shader.psModule)
        {
            fprintf(stderr,
                    "[ore_depth_sample] loadShader failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        auto layout0 =
            ore_gm::makeLayoutFromShader(ctx, shader.psModule.get(), 0);
        auto layout1 =
            ore_gm::makeLayoutFromShader(ctx, shader.psModule.get(), 1);
        BindGroupLayout* layouts[] = {layout0.get(), layout1.get()};

        PipelineDesc pipeDesc{};
        pipeDesc.vertexModule = shader.vsModule.get();
        pipeDesc.fragmentModule = shader.psModule.get();
        pipeDesc.vertexEntryPoint = shader.vsEntryPoint;
        pipeDesc.fragmentEntryPoint = shader.fsEntryPoint;
        pipeDesc.vertexBufferCount = 0;
        pipeDesc.topology = PrimitiveTopology::triangleList;
        pipeDesc.colorTargets[0].format = TextureFormat::rgba8unorm;
        pipeDesc.colorCount = 1;
        pipeDesc.bindGroupLayouts = layouts;
        pipeDesc.bindGroupLayoutCount = 2;
        pipeDesc.label = "ore_depth_sample_pipeline";
        std::string pipelineError;
        auto pipeline = ctx.makePipeline(pipeDesc, &pipelineError);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_depth_sample] pipeline creation failed: %s\n",
                    pipelineError.c_str());
            return;
        }

        BindGroupDesc::UBOEntry ubo0[1]{};
        ubo0[0].slot = 0;
        ubo0[0].buffer = uboBuf.get();
        ubo0[0].offset = 0;
        ubo0[0].size = sizeof(Uniforms);

        BindGroupDesc bg0Desc{};
        bg0Desc.layout = layout0.get();
        bg0Desc.ubos = ubo0;
        bg0Desc.uboCount = 1;
        bg0Desc.label = "depth_sample_bg_uniforms";
        auto bg0 = ctx.makeBindGroup(bg0Desc);
        if (!bg0)
        {
            fprintf(stderr,
                    "[ore_depth_sample] bg0 makeBindGroup failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        BindGroupDesc::TexEntry tex1[1]{};
        tex1[0].slot = 1;
        tex1[0].view = depthView.get();

        BindGroupDesc::SampEntry samp1[1]{};
        samp1[0].slot = 0;
        samp1[0].sampler = pointSampler.get();

        BindGroupDesc bg1Desc{};
        bg1Desc.layout = layout1.get();
        bg1Desc.textures = tex1;
        bg1Desc.textureCount = 1;
        bg1Desc.samplers = samp1;
        bg1Desc.samplerCount = 1;
        bg1Desc.label = "depth_sample_bg_textures";
        auto bg1 = ctx.makeBindGroup(bg1Desc);
        if (!bg1)
        {
            fprintf(stderr,
                    "[ore_depth_sample] bg1 makeBindGroup failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        auto canvas = renderContext->makeRenderCanvas(128, 128);
        if (!canvas)
        {
            fprintf(stderr, "[ore_depth_sample] makeRenderCanvas failed\n");
            return;
        }
        auto canvasTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!canvasTarget)
        {
            fprintf(stderr,
                    "[ore_depth_sample] wrapCanvasTexture failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        m_ore.beginFrame(renderContext);

        // Depth-only clear pass writes the known value the color pass reads.
        RenderPassDesc clearDesc{};
        clearDesc.colorCount = 0;
        clearDesc.depthStencil.view = depthView.get();
        clearDesc.depthStencil.depthLoadOp = LoadOp::clear;
        clearDesc.depthStencil.depthStoreOp = StoreOp::store;
        clearDesc.depthStencil.depthClearValue = 1.0f;
        clearDesc.label = "ore_depth_sample_clear_pass";
        auto clearPass = ctx.beginRenderPass(clearDesc);
        clearPass->finish();

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0].view = canvasTarget.get();
        rpDesc.colorAttachments[0].loadOp = LoadOp::clear;
        rpDesc.colorAttachments[0].storeOp = StoreOp::store;
        rpDesc.colorAttachments[0].clearColor = {1, 0, 0, 1};
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_depth_sample_pass";
        auto pass = ctx.beginRenderPass(rpDesc);
        pass->setPipeline(pipeline.get());
        pass->setBindGroup(0, bg0.get());
        pass->setBindGroup(1, bg1.get());
        pass->setViewport(0, 0, 128, 128);
        pass->draw(3);
        pass->finish();

        m_ore.endFrame(renderContext);
        ore_gm::invalidateGLStateAfterOre(renderContext);

        originalRenderer->save();
        originalRenderer->drawImage(canvas->renderImage(),
                                    {.filter = ImageFilter::nearest},
                                    BlendMode::srcOver,
                                    1);
        originalRenderer->restore();
#endif // ORE_DEPTH_SAMPLE_ACTIVE
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_depth_sample, return new OreDepthSampleGM)
