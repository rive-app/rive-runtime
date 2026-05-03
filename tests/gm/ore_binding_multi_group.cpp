/*
 * Copyright 2026 Rive
 *
 * Multi-group bind-group witness. Companion to `ore_binding_mixed_kind`
 * which exercises heterogeneous kinds *inside* one group; this one
 * exercises bind-group dispatch *across* groups — the second axis of
 * the RFC v5 §3.2.2 / §9.1 redesign.
 *
 * Shader:
 *
 *   @group(0) @binding(0) var<uniform> u_a: Uniforms;   // red
 *   @group(1) @binding(0) var<uniform> u_b: Uniforms;   // green
 *   fs_main → u_a.color.rgb + u_b.color.rgb   // = yellow
 *
 * The GM creates two BindGroups, one per group, and binds them with two
 * `setBindGroup` calls. Correct output is pure yellow (1, 1, 0, 1). Any
 * per-group routing bug (the second call clobbers the first group's
 * descriptors, or vice versa) appears as red, green, or black.
 *
 * Why this matters per backend (all must render yellow post-7a/7b/7c):
 *
 *   - Vulkan: `vkCmdBindDescriptorSets(firstSet=g, count=1, ...)` at
 *     different g values must bind independent descriptor sets. A bug
 *     in `m_vkPipelineLayout` (e.g. only one non-empty DSL) would
 *     collapse the two sets and show black.
 *   - D3D12: `SetGraphicsRootDescriptorTable(rootParam[g's table], ...)`
 *     must target the right root-parameter index per group. 7b's
 *     per-pipeline root sig has separate root params per group; if
 *     they'd been collapsed into one shared table (as the pre-7b
 *     context-wide root sig was), the second bind would clobber the
 *     first's UBO.
 *   - Metal: separate MTLBuffer slot ranges per group (v1 allocator
 *     assigns b0 and b1 for the two UBOs; both independent).
 *   - GL: v1 allocator gives global counter per kind, each UBO at its
 *     own binding point.
 *   - WebGPU: native bind groups, natural per-group dispatch.
 *
 * Known Metal behaviour: pixel-exact yellow (255, 255, 0, 255). Use
 * that as the cross-backend reference.
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
#define ORE_BINDING_MULTI_GROUP_ACTIVE
#endif

class OreBindingMultiGroupGM : public GM
{
public:
    OreBindingMultiGroupGM() : GM(128, 128) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_BINDING_MULTI_GROUP_ACTIVE
        auto& ctx = *m_ore.oreContext;

        // ── Two UBOs: u_a = red (1, 0, 0, 1), u_b = green (0, 1, 0, 1). ──
        // Sum in fragment shader = pure yellow (1, 1, 0, 1). Any per-group
        // bind-group clobber drops one of the additive colour channels.
        struct Uniforms
        {
            float r, g, b, a;
        };
        static const Uniforms kRed = {1.0f, 0.0f, 0.0f, 1.0f};
        static const Uniforms kGreen = {0.0f, 1.0f, 0.0f, 1.0f};

        BufferDesc uboADesc{};
        uboADesc.usage = BufferUsage::uniform;
        uboADesc.size = sizeof(Uniforms);
        uboADesc.data = &kRed;
        uboADesc.label = "multi_group_ubo_a";
        auto uboA = ctx.makeBuffer(uboADesc);

        BufferDesc uboBDesc{};
        uboBDesc.usage = BufferUsage::uniform;
        uboBDesc.size = sizeof(Uniforms);
        uboBDesc.data = &kGreen;
        uboBDesc.label = "multi_group_ubo_b";
        auto uboB = ctx.makeBuffer(uboBDesc);

        // ── Shader from the RSTB ──
        auto shader = ore_gm::loadShader(ctx, ore_gm::kMultiGroupWitness);
        if (!shader.vsModule)
            return;

        auto layout0 =
            ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 0);
        auto layout1 =
            ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 1);
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
        pipeDesc.depthStencil.depthCompare = CompareFunction::always;
        pipeDesc.depthStencil.depthWriteEnabled = false;
        pipeDesc.bindGroupLayouts = layouts;
        pipeDesc.bindGroupLayoutCount = 2;
        pipeDesc.label = "ore_binding_multi_group_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_binding_multi_group] pipeline creation failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // ── One BindGroup per group, one UBO in each. ──
        BindGroupDesc::UBOEntry aEntries[1]{};
        aEntries[0].slot = 0;
        aEntries[0].buffer = uboA.get();
        aEntries[0].offset = 0;
        aEntries[0].size = sizeof(Uniforms);

        BindGroupDesc bgADesc{};
        bgADesc.layout = layout0.get();
        bgADesc.ubos = aEntries;
        bgADesc.uboCount = 1;
        bgADesc.label = "multi_group_bg_a";
        auto bgA = ctx.makeBindGroup(bgADesc);
        if (!bgA)
            return;

        BindGroupDesc::UBOEntry bEntries[1]{};
        bEntries[0].slot = 0;
        bEntries[0].buffer = uboB.get();
        bEntries[0].offset = 0;
        bEntries[0].size = sizeof(Uniforms);

        BindGroupDesc bgBDesc{};
        bgBDesc.layout = layout1.get();
        bgBDesc.ubos = bEntries;
        bgBDesc.uboCount = 1;
        bgBDesc.label = "multi_group_bg_b";
        auto bgB = ctx.makeBindGroup(bgBDesc);
        if (!bgB)
            return;

        // ── Render into a RenderCanvas ──
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
        rpDesc.label = "ore_binding_multi_group_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass.setPipeline(pipeline.get());
        pass.setBindGroup(0, bgA.get());
        pass.setBindGroup(1, bgB.get());
        pass.setViewport(0, 0, 128, 128);
        pass.draw(3); // fullscreen triangle
        pass.finish();

        m_ore.endFrame();
        ore_gm::invalidateGLStateAfterOre(renderContext);

        originalRenderer->save();
        originalRenderer->drawImage(canvas->renderImage(),
                                    {.filter = ImageFilter::nearest},
                                    BlendMode::srcOver,
                                    1);
        originalRenderer->restore();
#endif // ORE_BINDING_MULTI_GROUP_ACTIVE
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_binding_multi_group, return new OreBindingMultiGroupGM)
