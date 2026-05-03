/*
 * Copyright 2026 Rive
 *
 * GM test / Phase 0 witness for the post-review fix sweep
 * (`/Users/luigi/Projects/plan/Ore/Post-Review Fix Plan.md` Phase 0
 * + Phase 1).
 *
 * This shader has the vertex stage sample a 1×1 green texture and
 * forward the sample as a varying. The fragment stage multiplies by a
 * UBO tint:
 *
 *   @group(0) @binding(0) var<uniform> u: Uniforms;   // FS-side
 *   @group(0) @binding(1) var t: texture_2d<f32>;     // VS-side sample
 *   @group(0) @binding(2) var s: sampler;             // VS-side sample
 *
 * With texture = (0, 1, 0, 1) green and tint = (1, 1, 1, 1) white,
 * correct output is pure green.
 *
 * Expected behavior on `ore_merge_squashed` today (PRE-FIX):
 *   - Metal: BLACK. `mtlSetBindGroup` (`ore_render_pass_metal.mm:139-146`)
 *     only emits `setFragmentTexture:` / `setFragmentSamplerState:` —
 *     never the vertex-stage equivalents. The VS sees an unbound texture
 *     and reads zeros; varying reaches FS as (0, 0, 0, 0); tint × 0 = 0.
 *   - Vulkan / D3D11 / D3D12 / GL / WGPU: GREEN. Their `makeBindGroup`
 *     also looks up `Stage::VS` only, but the v1 allocator stamps
 *     `[slot, slot, slot]` so VS happens to bind the right slot. This GM
 *     locks the post-fix invariant in so a future per-stage allocator
 *     can't regress them.
 *
 * Expected behavior POST-FIX (Phase 1 of the fix plan):
 *   - Every backend renders green.
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
#define ORE_BINDING_VS_TEXTURE_ACTIVE
#endif

class OreBindingVSTextureGM : public GM
{
public:
    OreBindingVSTextureGM() : GM(128, 128) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_BINDING_VS_TEXTURE_ACTIVE
        auto& ctx = *m_ore.oreContext;

        // ── UBO: white tint. Multiplier passes the VS sample through
        // unchanged on a correct bind. If FS-stage UBO binding regresses,
        // the shader multiplies by zero and the frame goes black anyway —
        // but the same-color failure mode is shared with the VS-texture
        // bug, so we rely on the rest of the matrix (mixed_kind etc.) to
        // disambiguate.
        struct Uniforms
        {
            float r, g, b, a;
        };
        static const Uniforms kTint = {1.0f, 1.0f, 1.0f, 1.0f};

        BufferDesc uboDesc{};
        uboDesc.usage = BufferUsage::uniform;
        uboDesc.size = sizeof(Uniforms);
        uboDesc.data = &kTint;
        uboDesc.label = "vs_texture_ubo";
        auto uboBuf = ctx.makeBuffer(uboDesc);

        // ── 1×1 green texture. The VS samples this; if the vertex-stage
        // texture binding is missing, the VS reads zeros and forwards
        // black to the FS regardless of the UBO tint.
        static const uint8_t kGreen[4] = {0x00, 0xFF, 0x00, 0xFF};

        TextureDesc texDesc{};
        texDesc.width = 1;
        texDesc.height = 1;
        texDesc.format = TextureFormat::rgba8unorm;
        texDesc.type = TextureType::texture2D;
        texDesc.numMipmaps = 1;
        texDesc.sampleCount = 1;
        texDesc.label = "vs_texture_texture";
        auto oreTex = ctx.makeTexture(texDesc);
        if (!oreTex)
            return;

        TextureDataDesc uploadDesc{};
        uploadDesc.data = kGreen;
        uploadDesc.width = 1;
        uploadDesc.height = 1;
        uploadDesc.bytesPerRow = 4;
        uploadDesc.rowsPerImage = 1;

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
        sampDesc.label = "vs_texture_sampler";
        auto sampler = ctx.makeSampler(sampDesc);

        // ── Shader from the RSTB ──
        auto shader = ore_gm::loadShader(ctx, ore_gm::kVSTextureWitness);
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
        pipeDesc.label = "ore_binding_vs_texture_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_binding_vs_texture] pipeline creation failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // ── Single BindGroup at groupIndex=0 holding all three kinds.
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
        bgDesc.label = "vs_texture_bg";
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
        rpDesc.label = "ore_binding_vs_texture_pass";

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
#endif // ORE_BINDING_VS_TEXTURE_ACTIVE
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_binding_vs_texture, return new OreBindingVSTextureGM)
