/*
 * Copyright 2025 Rive
 */

// GM test for Ore MRT (Multiple Render Targets). Renders a fullscreen quad
// with a shader that outputs different colors to 3 render targets, then
// composites all 3 side-by-side. Verifies: MRT render passes, multiple
// ColorAttachment, makeTexture with renderTarget, makeTextureView,
// and independent per-target output.

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
using namespace rive::ore;
#endif

class OreMrtGM : public GM
{
public:
    // 3 targets side-by-side, each 128x128 = 384x128 total.
    OreMrtGM() : GM(384, 128) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
        auto& ctx = *m_ore.oreContext;
        constexpr uint32_t kSize = 128;

        // Create 3 render target textures.
        rcp<ore::Texture> targets[3];
        rcp<ore::TextureView> targetViews[3];
        for (int i = 0; i < 3; ++i)
        {
            TextureDesc td{};
            td.width = kSize;
            td.height = kSize;
            td.format = TextureFormat::rgba8unorm;
            td.renderTarget = true;
            td.label = "mrt_target";
            targets[i] = ctx.makeTexture(td);

            TextureViewDesc tvd{};
            tvd.texture = targets[i].get();
            targetViews[i] = ctx.makeTextureView(tvd);
        }

        // Load MRT shader from the pre-compiled RSTB.
        auto mrtShader = ore_gm::loadShader(ctx, ore_gm::kMrt);
        if (!mrtShader.vsModule)
            return;

        PipelineDesc pipeDesc{};
        pipeDesc.vertexModule = mrtShader.vsModule.get();
        pipeDesc.fragmentModule = mrtShader.psModule.get();
        pipeDesc.vertexEntryPoint = mrtShader.vsEntryPoint;
        pipeDesc.fragmentEntryPoint = mrtShader.fsEntryPoint;
        pipeDesc.vertexBufferCount = 0; // No vertex buffers, use vertex_id.
        pipeDesc.topology = PrimitiveTopology::triangleList;
        pipeDesc.colorTargets[0].format = TextureFormat::rgba8unorm;
        pipeDesc.colorTargets[1].format = TextureFormat::rgba8unorm;
        pipeDesc.colorTargets[2].format = TextureFormat::rgba8unorm;
        pipeDesc.colorCount = 3;
        pipeDesc.depthStencil.depthCompare = CompareFunction::always;
        pipeDesc.depthStencil.depthWriteEnabled = false;
        pipeDesc.label = "mrt_pipeline";
        auto mrtPipeline = ctx.makePipeline(pipeDesc);

        // Render to all 3 targets in a single MRT pass.
        m_ore.beginFrame();

        RenderPassDesc rpDesc{};
        for (int i = 0; i < 3; ++i)
        {
            rpDesc.colorAttachments[i].view = targetViews[i].get();
            rpDesc.colorAttachments[i].loadOp = LoadOp::clear;
            rpDesc.colorAttachments[i].storeOp = StoreOp::store;
            rpDesc.colorAttachments[i].clearColor = {0, 0, 0, 1};
        }
        rpDesc.colorCount = 3;
        rpDesc.label = "mrt_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass.setPipeline(mrtPipeline.get());
        pass.setViewport(0, 0, kSize, kSize);
        pass.draw(3); // Fullscreen triangle, no VBO needed.
        pass.finish();

        // Now blit each target to a RenderCanvas for compositing.
        // Load blit shader from the pre-compiled RSTB.
        auto blitShader = ore_gm::loadShader(ctx, ore_gm::kMrtBlit);
        if (!blitShader.vsModule)
            return;

        auto blitLayout1 =
            ore_gm::makeLayoutFromShader(ctx, blitShader.vsModule.get(), 1);
        auto blitLayout2 =
            ore_gm::makeLayoutFromShader(ctx, blitShader.vsModule.get(), 2);
        BindGroupLayout* blitLayouts[] = {nullptr,
                                          blitLayout1.get(),
                                          blitLayout2.get()};

        PipelineDesc blitPipeDesc{};
        blitPipeDesc.vertexModule = blitShader.vsModule.get();
        blitPipeDesc.fragmentModule = blitShader.psModule.get();
        blitPipeDesc.vertexEntryPoint = blitShader.vsEntryPoint;
        blitPipeDesc.fragmentEntryPoint = blitShader.fsEntryPoint;
        blitPipeDesc.vertexBufferCount = 0;
        blitPipeDesc.topology = PrimitiveTopology::triangleList;
        blitPipeDesc.colorTargets[0].format = TextureFormat::rgba8unorm;
        blitPipeDesc.colorCount = 1;
        blitPipeDesc.depthStencil.depthCompare = CompareFunction::always;
        blitPipeDesc.depthStencil.depthWriteEnabled = false;
        blitPipeDesc.bindGroupLayouts = blitLayouts;
        blitPipeDesc.bindGroupLayoutCount = 3;
        blitPipeDesc.label = "blit_pipeline";
        auto blitPipeline = ctx.makePipeline(blitPipeDesc);

        SamplerDesc sampDesc{};
        sampDesc.minFilter = Filter::nearest;
        sampDesc.magFilter = Filter::nearest;
        sampDesc.label = "blit_sampler";
        auto sampler = ctx.makeSampler(sampDesc);

        // Create a wide canvas (384x128) and blit each target side-by-side.
        auto canvas = renderContext->makeRenderCanvas(384, kSize);
        if (!canvas)
            return;
        auto canvasView = ctx.wrapCanvasTexture(canvas.get());

        RenderPassDesc blitPassDesc{};
        blitPassDesc.colorAttachments[0].view = canvasView.get();
        blitPassDesc.colorAttachments[0].loadOp = LoadOp::clear;
        blitPassDesc.colorAttachments[0].storeOp = StoreOp::store;
        blitPassDesc.colorAttachments[0].clearColor = {0, 0, 0, 1};
        blitPassDesc.colorCount = 1;
        blitPassDesc.label = "blit_pass";

        // Create sampler bind group (shared across all blits).
        BindGroupDesc::SampEntry blitSampEntry{0, sampler.get()};
        BindGroupDesc blitSampBGDesc{};
        blitSampBGDesc.layout = blitLayout2.get();
        blitSampBGDesc.samplers = &blitSampEntry;
        blitSampBGDesc.samplerCount = 1;
        auto blitSampBG = ctx.makeBindGroup(blitSampBGDesc);

        // Blit all 3 targets side-by-side using viewport offsets.
        for (int i = 0; i < 3; ++i)
        {
            if (i > 0)
            {
                // Subsequent blits load (don't clear) the canvas.
                blitPassDesc.colorAttachments[0].loadOp = LoadOp::load;
            }

            // Create texture bind group per target.
            BindGroupDesc::TexEntry blitTexEntry{0, targetViews[i].get()};
            BindGroupDesc blitTexBGDesc{};
            blitTexBGDesc.layout = blitLayout1.get();
            blitTexBGDesc.textures = &blitTexEntry;
            blitTexBGDesc.textureCount = 1;
            auto blitTexBG = ctx.makeBindGroup(blitTexBGDesc);

            auto blitPass = ctx.beginRenderPass(blitPassDesc);
            blitPass.setPipeline(blitPipeline.get());
            blitPass.setBindGroup(1, blitTexBG.get());
            blitPass.setBindGroup(2, blitSampBG.get());
            blitPass.setViewport(i * kSize, 0, kSize, kSize);
            blitPass.draw(3);
            blitPass.finish();
        }

        m_ore.endFrame();
        ore_gm::invalidateGLStateAfterOre(renderContext);

        // Composite the canvas into the main framebuffer via the original
        // renderer. No frame break needed — Ore rendered to a separate canvas
        // texture, not the main render target.
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

GMREGISTER(ore_mrt, return new OreMrtGM())
