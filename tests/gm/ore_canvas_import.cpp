/*
 * Copyright 2026 Rive
 *
 * GM test for the Rive 2D RenderCanvas → Ore "imported canvas mirror"
 * boundary. See dev/ore_canvas_import_invariant.md for the architecture.
 *
 * What this verifies:
 *   1. We render an asymmetric pattern into a Rive 2D RenderCanvas via PLS:
 *        - top quarter:    bright green
 *        - bottom quarter: bright red
 *        - middle:         dark grey
 *      The asymmetry is the whole point — if the Y axis is wrong anywhere
 *      in the chain, green and red swap.
 *
 *   2. We import the source canvas's GPU texture into Ore via
 *      RenderContextGLImpl::getCanvasImportMirror() (GL/WebGL only):
 *        - On Metal/D3D/Vulkan/WebGPU the code is compiled out and we
 *          sample the source texture directly.
 *        - On GL/WebGL the GL impl returns a Y-flipped companion texture.
 *          A subsequent flush() of the source RenderCanvas will hardware
 *          blit the source into the companion with Y reversed.
 *
 *   3. We bind the chosen texture (mirror or source) into an Ore pipeline
 *      using the same image_view WGSL shader the ore_image_view GM uses
 *      (id = ore_gm::kImageView). The WGSL is authored with V=0 = visual
 *      top of the texture — i.e. the WGSL author's natural convention.
 *
 *   4. We render that pipeline into a separate Ore-managed RenderCanvas
 *      (the "destination"), and composite it into the main framebuffer
 *      via the original Rive renderer.
 *
 * On a correctly working backend, the destination should show:
 *        green at the top, dark grey in the middle, red at the bottom.
 * If the import-mirror is broken on GL the colors invert: red on top,
 * green at the bottom. If the WGSL UV convention is misinterpreted in
 * the shader the same inversion happens.
 *
 * The GM intentionally uses NO `1.0 - in.uv.y` workarounds in WGSL — the
 * whole point is to verify that the cross-backend invariant
 * "WGSL top of image at memory row 0" is preserved end-to-end.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "common/testing_window.hpp"

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_VK) || defined(ORE_BACKEND_WGPU) ||                    \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12)
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_render_image.hpp"
#if defined(ORE_BACKEND_GL)
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#endif
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_VK) || defined(ORE_BACKEND_WGPU) ||                    \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12)
using namespace rive::ore;
#endif

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_VK) || defined(ORE_BACKEND_WGPU) ||                    \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12)
#define ORE_CANVAS_IMPORT_ACTIVE
#endif

class OreCanvasImportGM : public GM
{
public:
    OreCanvasImportGM() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xff000000; } // black

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_CANVAS_IMPORT_ACTIVE
        constexpr uint32_t kSize = 256;

        // ── 1. Create the source Rive 2D RenderCanvas and render an
        //       asymmetric Y pattern into it via Rive PLS. ──
        //
        // Use the same intercept-and-resume pattern as render_canvas.cpp:
        // we hijack the active frame, end it, render into the canvas via
        // its own beginFrame/flush cycle, then resume the original frame
        // for the final composite.
        auto sourceCanvas = renderContext->makeRenderCanvas(kSize, kSize);
        if (!sourceCanvas)
            return;

        // Allocate the GL canvas-import mirror up front, *before* the
        // source canvas is flushed. The blit from source → mirror fires
        // from RenderContextGLImpl::flush() only when hasMirror == true,
        // so the mirror must exist at flush time. Lua gets this ordering
        // for free because :view() is called during bind-group setup
        // before canvas:endFrame(); the C++ GM has to do it explicitly.
        rcp<RiveRenderImage> mirrorImage;
#if defined(ORE_BACKEND_GL)
        if (renderContext->platformFeatures().framebufferBottomUp)
        {
            gpu::Texture* sourceTexEarly =
                sourceCanvas->renderImage()->getTexture();
            if (sourceTexEarly != nullptr)
            {
                auto* glImpl =
                    renderContext->static_impl_cast<gpu::RenderContextGLImpl>();
                mirrorImage =
                    glImpl->getCanvasImportMirror(sourceTexEarly, kSize, kSize);
            }
        }
#endif // ORE_BACKEND_GL

        auto originalFrameDescriptor = renderContext->frameDescriptor();
        TestingWindow::Get()->flushPLSContext();

        {
            auto canvasFD = originalFrameDescriptor;
            canvasFD.renderTargetWidth = kSize;
            canvasFD.renderTargetHeight = kSize;
            canvasFD.loadAction = gpu::LoadAction::clear;
            canvasFD.clearColor = 0xff202020; // dark grey
            renderContext->beginFrame(std::move(canvasFD));

            RiveRenderer renderer(renderContext);

            // Top quarter: bright green rectangle. (Rive pixel space:
            // y=0 is the visual top.)
            {
                Paint green(0xff00ff00);
                PathBuilder p;
                p.moveTo(0, 0);
                p.lineTo(kSize, 0);
                p.lineTo(kSize, kSize / 4.f);
                p.lineTo(0, kSize / 4.f);
                p.close();
                renderer.drawPath(p.detach(), green);
            }

            // Bottom quarter: bright red rectangle.
            {
                Paint red(0xffff0000);
                PathBuilder p;
                p.moveTo(0, kSize * 3.f / 4.f);
                p.lineTo(kSize, kSize * 3.f / 4.f);
                p.lineTo(kSize, kSize);
                p.lineTo(0, kSize);
                p.close();
                renderer.drawPath(p.detach(), red);
            }

            TestingWindow::Get()->flushPLSContext(sourceCanvas->renderTarget());
        }

        // Resume the main frame so the final composite at the bottom of
        // this method has a valid frame to draw into.
        {
            auto mainFD = originalFrameDescriptor;
            mainFD.loadAction = gpu::LoadAction::preserveRenderTarget;
            renderContext->beginFrame(std::move(mainFD));
        }

        // ── 2. Import the source canvas as an Ore-sampleable texture. ──
        //
        // On GL, the mirror was allocated up front (see top of this
        // method) and the blit from source → mirror fired during the
        // flushPLSContext(sourceCanvas) call above. The mirror is now
        // in sync with the source and ready to sample.
        gpu::Texture* sourceTex = sourceCanvas->renderImage()->getTexture();
        if (sourceTex == nullptr)
            return;

        gpu::Texture* texToWrap = sourceTex;
        if (mirrorImage != nullptr)
        {
            auto* mirrorRive =
                lite_rtti_cast<RiveRenderImage*>(mirrorImage.get());
            if (mirrorRive != nullptr && mirrorRive->getTexture() != nullptr)
            {
                texToWrap = mirrorRive->getTexture();
            }
        }

        // ── 3. Open the Ore frame. On Vulkan this connects Ore to the
        //       host's current command buffer so the texture we wrap below
        //       — and the draw that samples it — land in the same
        //       submission as Rive's writes into the source canvas.
        auto& ctx = *m_ore.oreContext;
        m_ore.beginFrame();

        // Wrap the chosen texture as an ore::TextureView. On Vulkan this
        // also emits a layout barrier (GENERAL → SHADER_READ_ONLY_OPTIMAL)
        // in the current CB and updates Rive's lastAccess tracking.
        auto sampledView = ctx.wrapRiveTexture(texToWrap, kSize, kSize);
        if (!sampledView)
            return;

        // ── 4. Build the Ore pipeline (image_view shader = id 2). ──
        auto shader = ore_gm::loadShader(ctx, ore_gm::kImageView);
        if (!shader.vsModule)
            return;

        SamplerDesc sampDesc{};
        sampDesc.minFilter = Filter::nearest;
        sampDesc.magFilter = Filter::nearest;
        sampDesc.label = "ore_canvas_import_sampler";
        auto sampler = ctx.makeSampler(sampDesc);

        // group 0 is empty for this shader; layouts at 1 and 2 are
        // textures + samplers respectively.
        rcp<BindGroupLayout> emptyLayout; // optional placeholder
        auto layout1 =
            ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 1);
        auto layout2 =
            ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 2);
        BindGroupLayout* layouts[] = {nullptr, layout1.get(), layout2.get()};

        PipelineDesc pipeDesc{};
        pipeDesc.vertexModule = shader.vsModule.get();
        pipeDesc.fragmentModule = shader.psModule.get();
        pipeDesc.vertexEntryPoint = shader.vsEntryPoint;
        pipeDesc.fragmentEntryPoint = shader.fsEntryPoint;
        pipeDesc.vertexBufferCount = 0;
        pipeDesc.topology = PrimitiveTopology::triangleList;
        pipeDesc.colorTargets[0].format = TextureFormat::rgba8unorm;
        pipeDesc.colorCount = 1;
        pipeDesc.depthStencil.depthCompare = CompareFunction::always;
        pipeDesc.depthStencil.depthWriteEnabled = false;
        pipeDesc.bindGroupLayouts = layouts;
        pipeDesc.bindGroupLayoutCount = 3;
        pipeDesc.label = "ore_canvas_import_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);
        if (!pipeline)
            return;

        BindGroupDesc texBGDesc{};
        texBGDesc.layout = layout1.get();
        BindGroupDesc::TexEntry texEntry{};
        texEntry.slot = 0;
        texEntry.view = sampledView.get();
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

        // ── 5. Render the Ore pass into a destination canvas. ──
        auto destCanvas = renderContext->makeRenderCanvas(kSize, kSize);
        if (!destCanvas)
            return;
        auto destView = ctx.wrapCanvasTexture(destCanvas.get());
        if (!destView)
            return;

        ColorAttachment ca{};
        ca.view = destView.get();
        ca.loadOp = LoadOp::clear;
        ca.storeOp = StoreOp::store;
        ca.clearColor = {0, 0, 0, 1};

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_canvas_import_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass.setPipeline(pipeline.get());
        pass.setBindGroup(1, texBG.get());
        pass.setBindGroup(2, sampBG.get());
        pass.setViewport(0, 0, kSize, kSize);
        pass.draw(6); // fullscreen quad
        pass.finish();

        m_ore.endFrame();
        ore_gm::invalidateGLStateAfterOre(renderContext);

        // ── 6. Composite the Ore canvas into the main framebuffer. ──
        originalRenderer->save();
        originalRenderer->drawImage(destCanvas->renderImage(),
                                    {.filter = ImageFilter::nearest},
                                    BlendMode::srcOver,
                                    1);
        originalRenderer->restore();
#endif // ORE_CANVAS_IMPORT_ACTIVE
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_canvas_import, return new OreCanvasImportGM)
