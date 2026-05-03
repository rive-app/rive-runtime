/*
 * Copyright 2026 Rive
 *
 * ── Twin GM of `ore_binding_shadow_sampler` — `depth32float` variant ──
 *
 * This GM is **byte-identical** to `ore_binding_shadow_sampler.cpp`
 * with exactly one deliberate difference:
 *
 *   Depth texture format: `depth32float` (no stencil) rather than
 *   `depth24plusStencil8` (combined depth-stencil).
 *
 * The view still declares `aspect=depthOnly` on a pure-depth format —
 * which is an identity no-op at the WebGPU spec level, but it is the
 * exact shape `SpinningCube.luau`'s shadow map uses in shipping Luau
 * content (`self.shadowDepthTex:view({ aspect = 'depthOnly' })` on a
 * `depth32f` texture). Everything else — shader, bind-group layout,
 * compare function, UBO contents, expected output — is the same as the
 * sibling GM.
 *
 * ── Why does this GM exist? ───────────────────────────────────────────
 *
 * Born from a 2026-04-24 incident where `ore_binding_shadow_sampler`
 * rendered black on `google_streamer_gl/wgpu` while every other adapter
 * (including `google_streamer_vk/wgpu` — same Ore code, same shader,
 * different Dawn-backed adapter) rendered green. The bug was on Ore's
 * side and was fixed in `280c48b70d` — `makeWGPUBGLEntry` was emitting
 * BindGroupLayoutEntry texture descriptors with hardcoded
 * `viewDimension=e2D, sampleType=Float`, which Dawn rejected at
 * `CreateRenderPipeline` time when reflected against a `texture_depth_2d`
 * shader (sampleType `Depth`); plus a secondary aspect-format mismatch
 * in `Context::makeTextureView` for `DepthOnly` views over D24S8. The
 * GM was hitting `if (!pipeline) return;` and the canvas's clear color
 * leaked through `drawImage`. The fix threads the shader-reflected
 * `TextureViewDim` and `TextureSampleType` through the binding map.
 *
 * This twin remains in the suite as a forward-looking one-variable
 * witness. Its shape mirrors `SpinningCube.luau`'s shipping shadow map
 * exactly (`depth32f` + `aspect=depthOnly`) — so if the WGPU shadow-
 * sampler path ever regresses again, the A/B against the sibling
 * triangulates the failure in one read:
 *
 *   Both green               → shadow-sampler path healthy.
 *   Only D24S8 GM black      → something specific to combined-DS +
 *                              aspect=depthOnly broke (BGL emission,
 *                              view-format selection, or driver
 *                              handling of D24S8 sampling).
 *                              SpinningCube-style content is SAFE.
 *   Only D32 GM black        → pure-depth shadow sampling regressed
 *                              independently. SpinningCube IS affected.
 *   Both black               → comparison-sampler / texture_depth
 *                              binding itself is broken (BGL sampleType
 *                              emission, WGSL→GLSL compile, or compare-
 *                              mode sampler state). Broadest impact.
 *
 * Five-minute triage instead of the multi-hour trip through driver-
 * state hypotheses it took the first time. The GM is 1×1 pixel's
 * worth of draws — it costs ~0 to keep in the suite.
 *
 * ── What this GM still witnesses on *every* backend ──────────────────
 *
 * The original `ore_binding_shadow_sampler` GM was added (commit
 * 7168f5e0e3) because `BindingMap::lookup` used to strictly distinguish
 * `ResourceKind::Sampler` from `ResourceKind::ComparisonSampler`, which
 * made `makeBindGroup` land `sampler_comparison` at the wrong backend
 * slot (raw WGSL `@binding` via the fallback path). On Metal that was a
 * driver SEGV inside `drawIndexedPrimitives`. This D32 twin exercises
 * the same bind-slot regression — the only thing that changed is the
 * texture format. If the BindingMap Sampler/ComparisonSampler collapse
 * ever regresses, both GMs will catch it.
 *
 * ── Expected output ──────────────────────────────────────────────────
 *
 *   Solid green — tint(0,1,0,1) × white × sampleCompareResult(1.0).
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
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
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
#define ORE_BINDING_SHADOW_SAMPLER_D32_ACTIVE
#endif

class OreBindingShadowSamplerD32GM : public GM
{
public:
    OreBindingShadowSamplerD32GM() : GM(128, 128) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_BINDING_SHADOW_SAMPLER_D32_ACTIVE
        auto& ctx = *m_ore.oreContext;

        // UBO: tint (green) + compare reference (0.25). The compare
        // reference is a hold-over from when the shader sampled with a
        // real depth-less compare function; with compare=always below,
        // the shader ignores it. Kept identical to the D24S8 GM so the
        // only varying factor across the twin pair is the texture shape.
        struct Uniforms
        {
            float tint[4];
            float ref[4];
        };
        static const Uniforms kU = {
            {0.0f, 1.0f, 0.0f, 1.0f},
            {0.25f, 0.0f, 0.0f, 0.0f},
        };
        BufferDesc uboDesc{};
        uboDesc.usage = BufferUsage::uniform;
        uboDesc.size = sizeof(Uniforms);
        uboDesc.data = &kU;
        uboDesc.label = "shadow_sampler_d32_ubo";
        auto uboBuf = ctx.makeBuffer(uboDesc);

        // 1x1 white texture for the regular (non-compare) sample path.
        static const uint8_t kWhite[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        TextureDesc colorDesc{};
        colorDesc.width = 1;
        colorDesc.height = 1;
        colorDesc.format = TextureFormat::rgba8unorm;
        colorDesc.type = TextureType::texture2D;
        colorDesc.numMipmaps = 1;
        colorDesc.sampleCount = 1;
        colorDesc.label = "shadow_sampler_d32_color";
        auto colorTex = ctx.makeTexture(colorDesc);
        if (!colorTex)
        {
            fprintf(stderr,
                    "[shadow_sampler_d32] colorTex makeTexture failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }
        TextureDataDesc colorUpload{};
        colorUpload.data = kWhite;
        colorUpload.width = 1;
        colorUpload.height = 1;
        colorUpload.bytesPerRow = 4;
        colorUpload.rowsPerImage = 1;

        TextureViewDesc colorViewDesc{};
        colorViewDesc.texture = colorTex.get();
        colorViewDesc.dimension = TextureViewDimension::texture2D;
        colorViewDesc.baseMipLevel = 0;
        colorViewDesc.mipCount = 1;
        colorViewDesc.baseLayer = 0;
        colorViewDesc.layerCount = 1;
        auto colorView = ctx.makeTextureView(colorViewDesc);
        if (!colorView)
        {
            fprintf(stderr,
                    "[shadow_sampler_d32] colorView makeTextureView "
                    "failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // ─── The only difference from the sibling GM ─────────────────
        //
        // Pure `depth32float` instead of `depth24plusStencil8`. The
        // view still uses `aspect=depthOnly` — identity on a pure-depth
        // format at the WebGPU spec level, but kept so this GM mirrors
        // SpinningCube.luau's shadow-map shape exactly:
        //   `self.shadowDepthTex:view({ aspect = 'depthOnly' })` on a
        //   `depth32f` texture.
        // `renderTarget=true` stays because the sibling has it; we
        // never actually write into the texture (compare=always makes
        // the stored value irrelevant).
        TextureDesc depthDesc{};
        depthDesc.width = 1;
        depthDesc.height = 1;
        depthDesc.format = TextureFormat::depth32float;
        depthDesc.type = TextureType::texture2D;
        depthDesc.renderTarget = true;
        depthDesc.numMipmaps = 1;
        depthDesc.sampleCount = 1;
        depthDesc.label = "shadow_sampler_d32_depth";
        auto depthTex = ctx.makeTexture(depthDesc);
        if (!depthTex)
        {
            fprintf(stderr,
                    "[shadow_sampler_d32] depthTex makeTexture failed: %s\n",
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
        depthViewDesc.aspect = TextureAspect::depthOnly;
        auto depthView = ctx.makeTextureView(depthViewDesc);
        if (!depthView)
        {
            fprintf(stderr,
                    "[shadow_sampler_d32] depthView makeTextureView "
                    "(depthOnly aspect on depth32float) failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }
        // ──────────────────────────────────────────────────────────────

        SamplerDesc sampDesc{};
        sampDesc.minFilter = Filter::nearest;
        sampDesc.magFilter = Filter::nearest;
        sampDesc.label = "shadow_sampler_d32_tex";
        auto texSampler = ctx.makeSampler(sampDesc);

        // Comparison sampler: compare=always. `textureSampleCompare`
        // returns 1.0 regardless of stored depth. Same rationale as in
        // the sibling GM — this is a bind-slot regression witness, not
        // a depth-compare correctness test.
        SamplerDesc compareDesc{};
        compareDesc.minFilter = Filter::nearest;
        compareDesc.magFilter = Filter::nearest;
        compareDesc.compare = CompareFunction::always;
        compareDesc.label = "shadow_sampler_d32_compare";
        auto compareSampler = ctx.makeSampler(compareDesc);

        // Shared shader with the sibling GM. The WGSL side declares the
        // shadow map as `texture_depth_2d` — that's the same type
        // whether the underlying resource is D32F or D24S8+depthOnly.
        auto shader = ore_gm::loadShader(ctx, ore_gm::kShadowSamplerWitness);
        if (!shader.vsModule)
        {
            fprintf(stderr,
                    "[shadow_sampler_d32] loadShader failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

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
        pipeDesc.label = "ore_binding_shadow_sampler_d32_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_binding_shadow_sampler_d32] pipeline creation "
                    "failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // Bind groups identical to the sibling GM:
        //   group 0 = UBO alone.
        //   group 1 = {texSampler, texColor, shadowSampler, shadowMap}.
        BindGroupDesc::UBOEntry ubo0[1]{};
        ubo0[0].slot = 0;
        ubo0[0].buffer = uboBuf.get();
        ubo0[0].offset = 0;
        ubo0[0].size = sizeof(Uniforms);

        BindGroupDesc bg0Desc{};
        bg0Desc.layout = layout0.get();
        bg0Desc.ubos = ubo0;
        bg0Desc.uboCount = 1;
        bg0Desc.label = "shadow_sampler_d32_bg_uniforms";
        auto bg0 = ctx.makeBindGroup(bg0Desc);
        if (!bg0)
        {
            fprintf(stderr,
                    "[shadow_sampler_d32] bg0 makeBindGroup failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        BindGroupDesc::TexEntry tex1[2]{};
        tex1[0].slot = 1;
        tex1[0].view = colorView.get();
        tex1[1].slot = 3;
        tex1[1].view = depthView.get();

        BindGroupDesc::SampEntry samp1[2]{};
        samp1[0].slot = 0;
        samp1[0].sampler = texSampler.get();
        samp1[1].slot = 2;
        samp1[1].sampler = compareSampler.get();

        BindGroupDesc bg1Desc{};
        bg1Desc.layout = layout1.get();
        bg1Desc.textures = tex1;
        bg1Desc.textureCount = 2;
        bg1Desc.samplers = samp1;
        bg1Desc.samplerCount = 2;
        bg1Desc.label = "shadow_sampler_d32_bg_textures";
        auto bg1 = ctx.makeBindGroup(bg1Desc);
        if (!bg1)
        {
            fprintf(stderr,
                    "[shadow_sampler_d32] bg1 makeBindGroup failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        auto canvas = renderContext->makeRenderCanvas(128, 128);
        if (!canvas)
        {
            fprintf(stderr, "[shadow_sampler_d32] makeRenderCanvas failed\n");
            return;
        }
        auto canvasTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!canvasTarget)
        {
            fprintf(stderr,
                    "[shadow_sampler_d32] wrapCanvasTexture failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        m_ore.beginFrame();
        colorTex->upload(colorUpload);

        // Single pass: bind both groups + draw a fullscreen triangle.
        // With compare=always the sample_compare result is 1.0 so the
        // fragment output is tint (green) × white × 1 = green.
        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0].view = canvasTarget.get();
        rpDesc.colorAttachments[0].loadOp = LoadOp::clear;
        rpDesc.colorAttachments[0].storeOp = StoreOp::store;
        rpDesc.colorAttachments[0].clearColor = {0, 0, 0, 1};
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_binding_shadow_sampler_d32_pass";
        auto pass = ctx.beginRenderPass(rpDesc);
        pass.setPipeline(pipeline.get());
        pass.setBindGroup(0, bg0.get());
        pass.setBindGroup(1, bg1.get());
        pass.setViewport(0, 0, 128, 128);
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
#endif // ORE_BINDING_SHADOW_SAMPLER_D32_ACTIVE
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_binding_shadow_sampler_d32,
           return new OreBindingShadowSamplerD32GM)
