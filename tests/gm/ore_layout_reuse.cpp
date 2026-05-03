/*
 * Copyright 2026 Rive
 *
 * Phase E witness: ONE BindGroupLayout used by TWO pipelines + ONE BindGroup
 * shared across them. Proves the layout/pipeline/bindgroup decoupling is
 * actually decoupled — pre-Phase-E this scenario was impossible because
 * BindGroups were tied to a specific Pipeline.
 *
 * Setup:
 *   - One shader (`binding_witness`) with two UBOs at @group(0) @binding(0,7).
 *   - One BindGroupLayout derived from the shader, shared across both
 *     pipelines.
 *   - Two pipelines that differ only in color-write mask — one writes RGBA,
 *     the other writes only the red channel. Both reference the SAME layout.
 *   - One BindGroup with the two UBOs (low.r=0.3, high.g=0.6) against the
 *     shared layout.
 *
 * Renders:
 *   - Left half (64×128): full-RGBA pipeline → olive (0.3, 0.6, 0, 1).
 *   - Right half (64×128): red-only pipeline → red (0.3, 0, 0, 0) on top of
 *     the cleared-black background → dark red.
 *
 * Pre-Phase-E: not expressible (single BindGroup couldn't be used with two
 * pipelines). Post-Phase-E: works because both pipelines share the same
 * layout, and the BindGroup conforms to that one layout.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_VK) || defined(ORE_BACKEND_WGPU) ||                    \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12)
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
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
#define ORE_LAYOUT_REUSE_ACTIVE
#endif

class OreLayoutReuseGM : public GM
{
public:
    OreLayoutReuseGM() : GM(128, 128) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_LAYOUT_REUSE_ACTIVE
        auto& ctx = *m_ore.oreContext;

        struct Uniforms
        {
            float r, g, b, a;
        };
        static const Uniforms kLow = {0.3f, 0.0f, 0.0f, 0.0f};
        static const Uniforms kHigh = {0.0f, 0.6f, 0.0f, 0.0f};

        BufferDesc lowDesc{};
        lowDesc.usage = BufferUsage::uniform;
        lowDesc.size = sizeof(Uniforms);
        lowDesc.data = &kLow;
        lowDesc.label = "reuse_ubo_low";
        auto lowBuf = ctx.makeBuffer(lowDesc);

        BufferDesc highDesc{};
        highDesc.usage = BufferUsage::uniform;
        highDesc.size = sizeof(Uniforms);
        highDesc.data = &kHigh;
        highDesc.label = "reuse_ubo_high";
        auto highBuf = ctx.makeBuffer(highDesc);

        auto shader = ore_gm::loadShader(ctx, ore_gm::kBindingWitness);
        if (!shader.vsModule)
            return;

        // ── ONE layout, used by both pipelines. ──
        auto sharedLayout =
            ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 0);
        if (!sharedLayout)
            return;
        BindGroupLayout* layouts[] = {sharedLayout.get()};

        // Pipeline A — full RGBA write.
        PipelineDesc pipeADesc{};
        pipeADesc.vertexModule = shader.vsModule.get();
        pipeADesc.fragmentModule = shader.psModule.get();
        pipeADesc.vertexEntryPoint = shader.vsEntryPoint;
        pipeADesc.fragmentEntryPoint = shader.fsEntryPoint;
        pipeADesc.vertexBufferCount = 0;
        pipeADesc.topology = PrimitiveTopology::triangleList;
        pipeADesc.colorTargets[0].format = TextureFormat::rgba8unorm;
        pipeADesc.colorTargets[0].writeMask = ColorWriteMask::all;
        pipeADesc.colorCount = 1;
        pipeADesc.depthStencil.depthCompare = CompareFunction::always;
        pipeADesc.depthStencil.depthWriteEnabled = false;
        pipeADesc.bindGroupLayouts = layouts;
        pipeADesc.bindGroupLayoutCount = 1;
        pipeADesc.label = "layout_reuse_pipeline_full";
        auto pipeA = ctx.makePipeline(pipeADesc);
        if (!pipeA)
        {
            fprintf(stderr,
                    "[ore_layout_reuse] pipeA failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // Pipeline B — red-only write mask. Same layout, different state.
        PipelineDesc pipeBDesc = pipeADesc;
        pipeBDesc.colorTargets[0].writeMask = ColorWriteMask::red;
        pipeBDesc.label = "layout_reuse_pipeline_red";
        auto pipeB = ctx.makePipeline(pipeBDesc);
        if (!pipeB)
        {
            fprintf(stderr,
                    "[ore_layout_reuse] pipeB failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // ── ONE BindGroup, conformed to the shared layout. Used with both
        // pipelines below. Pre-Phase-E this required two BindGroups (one
        // per pipeline). Now: one suffices.
        BindGroupDesc::UBOEntry uboEntries[2]{};
        uboEntries[0].slot = 0;
        uboEntries[0].buffer = lowBuf.get();
        uboEntries[0].size = sizeof(Uniforms);
        uboEntries[1].slot = 7;
        uboEntries[1].buffer = highBuf.get();
        uboEntries[1].size = sizeof(Uniforms);

        BindGroupDesc bgDesc{};
        bgDesc.layout = sharedLayout.get();
        bgDesc.ubos = uboEntries;
        bgDesc.uboCount = 2;
        bgDesc.label = "layout_reuse_bg";
        auto sharedBG = ctx.makeBindGroup(bgDesc);
        if (!sharedBG)
            return;

        auto canvas = renderContext->makeRenderCanvas(128, 128);
        if (!canvas)
            return;
        auto canvasTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!canvasTarget)
            return;

        m_ore.beginFrame();

        ColorAttachment ca{};
        ca.view = canvasTarget.get();
        ca.loadOp = LoadOp::clear;
        ca.storeOp = StoreOp::store;
        ca.clearColor = {0, 0, 0, 1};

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_layout_reuse_pass";

        auto pass = ctx.beginRenderPass(rpDesc);

        // Left half: pipe A (full RGBA) — should produce olive (0.3, 0.6, 0,
        // 1).
        pass.setPipeline(pipeA.get());
        pass.setBindGroup(0, sharedBG.get());
        pass.setViewport(0, 0, 64, 128);
        pass.draw(3);

        // Right half: pipe B (red-only mask) — same shared bind group.
        // Output: dark red (0.3, 0, 0, 1).
        pass.setPipeline(pipeB.get());
        pass.setBindGroup(0, sharedBG.get());
        pass.setViewport(64, 0, 64, 128);
        pass.draw(3);

        pass.finish();

        m_ore.endFrame();
        ore_gm::invalidateGLStateAfterOre(renderContext);

        originalRenderer->save();
        originalRenderer->drawImage(canvas->renderImage(),
                                    {.filter = ImageFilter::nearest},
                                    BlendMode::srcOver,
                                    1);
        originalRenderer->restore();
#endif // ORE_LAYOUT_REUSE_ACTIVE
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_layout_reuse, return new OreLayoutReuseGM)
