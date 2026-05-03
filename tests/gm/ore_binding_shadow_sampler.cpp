/*
 * Copyright 2026 Rive
 *
 * GM test / regression witness for BindingMap's Sampler vs
 * ComparisonSampler kind collapse (commit 7168f5e0e3).
 *
 * Pre-fix: every flatten-backend's `makeBindGroup` called
 * `BindingMap::lookup(..., ResourceKind::Sampler, ...)` for every
 * `SampEntry`. The allocator stored WGSL `sampler_comparison`
 * declarations as `ResourceKind::ComparisonSampler`, so the strict-
 * kind lookup returned `kAbsent`, mapSlot fell back to the raw
 * `@binding`, and the sampler landed at the wrong backend slot. On
 * Metal this manifested as a driver SEGV inside
 * `drawIndexedPrimitives` because the shader sampled an unbound
 * `[[sampler(N)]]`.
 *
 * Post-fix: `BindingMap::lookup` treats Sampler and ComparisonSampler
 * as interchangeable. The sampler lands at the allocator's chosen
 * slot, the shader reads it, and the draw succeeds.
 *
 * Shape of the test:
 *   1. Create a 1x1 depth24plusStencil8 texture with aspect=depthOnly
 *      view (mirrors SpinningCube.luau's shadow-map usage shape).
 *   2. Fullscreen triangle samples the depth texture via a
 *      `sampler_comparison` whose `compare=always`. Per WebGPU spec,
 *      `textureSampleCompare` returns 1.0 unconditionally regardless
 *      of the (uninitialised) stored depth, so this GM is a pure
 *      bind-slot / layout regression test — not a depth-compare
 *      correctness test.
 *   3. UBO tint = green. Correct output = solid green.
 *      Pre-7168f5e0e3 = Metal driver crash.
 *
 * ── Twin GM ──────────────────────────────────────────────────────────
 *
 * `ore_binding_shadow_sampler_d32.cpp` is a one-variable twin of this
 * GM — same shader, same bind layout, same compare=always, same
 * `aspect=depthOnly` — but uses `depth32float` instead of
 * `depth24plusStencil8`. That shape mirrors `SpinningCube.luau`'s
 * shipping shadow map exactly, so the twin doubles as a real-content
 * safety check. Keeping both in the suite lets anyone narrow future
 * adapter-specific discrepancies in the shadow-sampler path in five
 * minutes:
 *
 *   Both green  → shadow-sampler path healthy.
 *   D24S8 black → something specific to combined depth-stencil +
 *                 aspect=depthOnly broke (Ore-side BGL emission for
 *                 the depth-aspect view, or driver mishandling of
 *                 Depth24PlusStencil8 sampling). SpinningCube-style
 *                 `depth32f`+depthOnly content is SAFE.
 *   D32 black   → pure-depth shadow sampling regressed independently;
 *                 SpinningCube IS affected.
 *   Both black  → comparison-sampler / texture_depth binding itself
 *                 broken (BGL sampleType emission, WGSL→GLSL compile,
 *                 or compare-mode sampler state) — broadest impact.
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
#define ORE_BINDING_SHADOW_SAMPLER_ACTIVE
#endif

class OreBindingShadowSamplerGM : public GM
{
public:
    OreBindingShadowSamplerGM() : GM(128, 128) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_BINDING_SHADOW_SAMPLER_ACTIVE
        auto& ctx = *m_ore.oreContext;

        // UBO: tint (green) + compare reference (0.25).
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
        uboDesc.label = "shadow_sampler_ubo";
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
        colorDesc.label = "shadow_sampler_color";
        auto colorTex = ctx.makeTexture(colorDesc);
        if (!colorTex)
            return;
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

        // depth24plusStencil8 texture with aspect=depthOnly view —
        // mirrors SpinningCube.luau's `self.shadowDepthTex:view({
        // aspect = 'depthOnly' })` pattern. renderTarget=true is
        // required; we don't write into it (compare=always).
        TextureDesc depthDesc{};
        depthDesc.width = 1;
        depthDesc.height = 1;
        depthDesc.format = TextureFormat::depth24plusStencil8;
        depthDesc.type = TextureType::texture2D;
        depthDesc.renderTarget = true;
        depthDesc.numMipmaps = 1;
        depthDesc.sampleCount = 1;
        depthDesc.label = "shadow_sampler_depth";
        auto depthTex = ctx.makeTexture(depthDesc);
        if (!depthTex)
            return;

        TextureViewDesc depthViewDesc{};
        depthViewDesc.texture = depthTex.get();
        depthViewDesc.dimension = TextureViewDimension::texture2D;
        depthViewDesc.baseMipLevel = 0;
        depthViewDesc.mipCount = 1;
        depthViewDesc.baseLayer = 0;
        depthViewDesc.layerCount = 1;
        depthViewDesc.aspect = TextureAspect::depthOnly;
        auto depthView = ctx.makeTextureView(depthViewDesc);

        SamplerDesc sampDesc{};
        sampDesc.minFilter = Filter::nearest;
        sampDesc.magFilter = Filter::nearest;
        sampDesc.label = "shadow_sampler_tex";
        auto texSampler = ctx.makeSampler(sampDesc);

        // Comparison sampler: compare=always. `textureSampleCompare`
        // returns 1.0 regardless of stored depth. Using `always` side-
        // steps any stored-value initialization concerns — this GM is
        // a bind-slot regression test, not a depth-compare correctness
        // test.
        SamplerDesc compareDesc{};
        compareDesc.minFilter = Filter::nearest;
        compareDesc.magFilter = Filter::nearest;
        compareDesc.compare = CompareFunction::always;
        compareDesc.label = "shadow_sampler_compare";
        auto compareSampler = ctx.makeSampler(compareDesc);

        // Shader from the RSTB.
        auto shader = ore_gm::loadShader(ctx, ore_gm::kShadowSamplerWitness);
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
        pipeDesc.label = "ore_binding_shadow_sampler_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_binding_shadow_sampler] pipeline creation failed: "
                    "%s\n",
                    ctx.lastError().c_str());
            return;
        }

        // Mirror torus_shader.wgsl's exact bind group split:
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
        bg0Desc.label = "shadow_sampler_bg_uniforms";
        auto bg0 = ctx.makeBindGroup(bg0Desc);
        if (!bg0)
            return;

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
        bg1Desc.label = "shadow_sampler_bg_textures";
        auto bg1 = ctx.makeBindGroup(bg1Desc);
        if (!bg1)
            return;

        auto canvas = renderContext->makeRenderCanvas(128, 128);
        if (!canvas)
            return;
        auto canvasTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!canvasTarget)
            return;

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
        rpDesc.label = "ore_binding_shadow_sampler_pass";
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
#endif // ORE_BINDING_SHADOW_SAMPLER_ACTIVE
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_binding_shadow_sampler, return new OreBindingShadowSamplerGM)
