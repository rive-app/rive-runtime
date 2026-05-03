/*
 * Copyright 2026 Rive
 *
 * GM test / regression witness for the bind-group redesign's remaining
 * per-backend work (RFC v4 §9.1 — the "Commit 7" items not yet landed
 * on Vulkan, D3D12, or WebGPU).
 *
 * This shader declares three resources of three different kinds inside
 * a single bind group:
 *
 *   @group(0) @binding(0) var<uniform> u: Uniforms;
 *   @group(0) @binding(1) var t: texture_2d<f32>;
 *   @group(0) @binding(2) var s: sampler;
 *
 * Every other GM on-branch uses the legacy kind-per-group convention
 * (UBOs in group 0, textures in group 1, samplers in group 2) which is
 * exactly the fingerprint of the pre-redesign allocator, so the v1
 * allocator's output happens to match each backend's kind-partitioned
 * runtime DSL shape. Those GMs can't tell whether a backend supports
 * idiomatic WebGPU group semantics or not.
 *
 * This one can. A correct binding renders pure green:
 *
 *   UBO.color   = (1.0, 1.0, 0.0, 1.0)   // yellow
 *   texture 1x1 = (0.0, 1.0, 1.0, 1.0)   // cyan
 *   output      = u.color.rgb * sample.rgb = (0.0, 1.0, 0.0, 1.0)
 *
 * Expected state on `ore_merge_squashed` today:
 *   - Metal, Metal-GL, pure GL, D3D11: render green.
 *   - Vulkan: fails validation / misrenders. createDSL in
 *     ore_pipeline_vulkan.cpp builds three homogeneous sets
 *     (set 0 = 8 UBOs, set 1 = 8 tex, set 2 = 8 samplers). Writing the
 *     texture + sampler descriptors into set 0 carries the wrong
 *     VkDescriptorType for this DSL.
 *   - WebGPU: fails / misrenders. m_wgpuBGL[] is populated via
 *     GetBindGroupLayout(g) on a layout:"auto" pipeline and indexed by
 *     groupIndex assuming g==0 is the UBO-only layout.
 *   - D3D12: renders green accidentally today — current D3D12 root
 *     signature has every kind at RegisterSpace=0 and the v1 HLSL
 *     SM5.0 allocator's global-counter register numbers happen to line
 *     up for a single-group shader. A two-group variant of this
 *     witness (follow-up) will expose the per-group-space gap.
 *
 * Once the Commit 7 work lands (per-group heterogeneous DSLs on
 * Vulkan, explicit bind-group layouts on WebGPU, per-pipeline root
 * signatures with per-group descriptor tables on D3D12), this GM
 * renders green on every backend and locks in the regression test.
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
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
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
#define ORE_BINDING_MIXED_KIND_ACTIVE
#endif

class OreBindingMixedKindGM : public GM
{
public:
    OreBindingMixedKindGM() : GM(128, 128) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_BINDING_MIXED_KIND_ACTIVE
        auto& ctx = *m_ore.oreContext;

        // ── UBO: yellow (1, 1, 0, 1). Shader multiplies this by the
        // sampled texel, so a wrong UBO bind zeroes the red or green
        // channel and the output stops being pure green.
        struct Uniforms
        {
            float r, g, b, a;
        };
        static const Uniforms kColor = {1.0f, 1.0f, 0.0f, 1.0f};

        BufferDesc uboDesc{};
        uboDesc.usage = BufferUsage::uniform;
        uboDesc.size = sizeof(Uniforms);
        uboDesc.data = &kColor;
        uboDesc.label = "mixed_kind_ubo";
        auto uboBuf = ctx.makeBuffer(uboDesc);

        // ── 1×1 cyan texture. Multiplying yellow by cyan gives pure
        // green; any wrong texture bind (or a texture bound to a UBO
        // slot) makes the fragment sample garbage/zero and the output
        // stops being green.
        static const uint8_t kCyan[4] = {0x00, 0xFF, 0xFF, 0xFF};

        TextureDesc texDesc{};
        texDesc.width = 1;
        texDesc.height = 1;
        texDesc.format = TextureFormat::rgba8unorm;
        texDesc.type = TextureType::texture2D;
        texDesc.numMipmaps = 1;
        texDesc.sampleCount = 1;
        texDesc.label = "mixed_kind_texture";
        auto oreTex = ctx.makeTexture(texDesc);
        if (!oreTex)
            return;

        TextureDataDesc uploadDesc{};
        uploadDesc.data = kCyan;
        uploadDesc.width = 1;
        uploadDesc.height = 1;
        uploadDesc.bytesPerRow = 4;
        uploadDesc.rowsPerImage = 1;
        // Upload happens after beginFrame() below — Texture::upload records
        // its barriers + copy into the context's command buffer, which must
        // already be in the recording state.

        TextureViewDesc tvDesc{};
        tvDesc.texture = oreTex.get();
        tvDesc.dimension = TextureViewDimension::texture2D;
        tvDesc.baseMipLevel = 0;
        tvDesc.mipCount = 1;
        tvDesc.baseLayer = 0;
        tvDesc.layerCount = 1;
        auto texView = ctx.makeTextureView(tvDesc);
        if (!texView)
            return;

        SamplerDesc sampDesc{};
        sampDesc.minFilter = Filter::nearest;
        sampDesc.magFilter = Filter::nearest;
        sampDesc.label = "mixed_kind_sampler";
        auto sampler = ctx.makeSampler(sampDesc);

        // ── Shader from the RSTB ──
        auto shader = ore_gm::loadShader(ctx, ore_gm::kMixedKindWitness);
        if (!shader.vsModule)
            return;

        auto layout0 =
            ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 0);
        BindGroupLayout* layouts[] = {layout0.get()};

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
        pipeDesc.bindGroupLayoutCount = 1;
        pipeDesc.label = "ore_binding_mixed_kind_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_binding_mixed_kind] pipeline creation failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // ── Single BindGroup at groupIndex=0 holding all three kinds.
        // This is the shape RFC v4 §9.1 mandates and that current
        // Vulkan/WebGPU runtimes can't accept.
        BindGroupDesc::UBOEntry uboEntries[1]{};
        uboEntries[0].slot = 0;
        uboEntries[0].buffer = uboBuf.get();
        uboEntries[0].offset = 0;
        uboEntries[0].size = sizeof(Uniforms);

        BindGroupDesc::TexEntry texEntries[1]{};
        texEntries[0].slot = 1;
        texEntries[0].view = texView.get();

        BindGroupDesc::SampEntry sampEntries[1]{};
        sampEntries[0].slot = 2;
        sampEntries[0].sampler = sampler.get();

        BindGroupDesc bgDesc{};
        bgDesc.layout = layout0.get();
        bgDesc.ubos = uboEntries;
        bgDesc.uboCount = 1;
        bgDesc.textures = texEntries;
        bgDesc.textureCount = 1;
        bgDesc.samplers = sampEntries;
        bgDesc.samplerCount = 1;
        bgDesc.label = "mixed_kind_bg";
        auto bg = ctx.makeBindGroup(bgDesc);
        if (!bg)
            return;

        // ── Render into a RenderCanvas ──
        auto canvas = renderContext->makeRenderCanvas(128, 128);
        if (!canvas)
            return;

        auto canvasTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!canvasTarget)
            return;

        m_ore.beginFrame();

        oreTex->upload(uploadDesc);

        ColorAttachment ca{};
        ca.view = canvasTarget.get();
        ca.loadOp = LoadOp::clear;
        ca.storeOp = StoreOp::store;
        ca.clearColor = {0, 0, 0, 1};

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_binding_mixed_kind_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass.setPipeline(pipeline.get());
        pass.setBindGroup(0, bg.get());
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
#endif // ORE_BINDING_MIXED_KIND_ACTIVE
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_binding_mixed_kind, return new OreBindingMixedKindGM)
