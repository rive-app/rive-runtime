/*
 * Copyright 2026 Rive
 *
 * GM test / Phase 4 witness for the post-review fix sweep
 * (`/Users/luigi/Projects/plan/Ore/Post-Review Fix Plan.md` Phase 4).
 *
 * Locks two latent upload-path bugs that the existing GMs all missed:
 *
 *   1. D3D12 subresource formula. The transposed pre-fix formula
 *      `mipLevel * ArraySize + layer` only happens to coincide with the
 *      correct `mipLevel + layer * MipLevels` when `MipLevels == 1` —
 *      which every other Ore GM satisfies. This GM creates a 4×4
 *      `array2D` texture with **`numMipmaps = 2`** so the two formulas
 *      diverge: the correct formula maps `(layer=1, mip=0)` to
 *      subresource 2, the transposed formula maps it to subresource 1
 *      (mip 1 of layer 0) and silently mis-targets the upload.
 *
 *   2. GL `bytesPerRow` honoured via `GL_UNPACK_ROW_LENGTH`. We upload
 *      each layer with a **non-tightly-packed** source (`bytesPerRow =
 *      32` bytes for a 4-pixel-wide RGBA8 row, which is the natural 16
 *      bytes plus 16 bytes of padding). Without the
 *      `GL_UNPACK_ROW_LENGTH` plumbing, GL reads tightly and the
 *      uploaded layer comes back as the first pixel repeated four
 *      times in stripes (or just garbage) instead of the intended
 *      solid-color square.
 *
 * Layer colors (mip 0):
 *   layer 0 → red    (255,   0,   0)
 *   layer 1 → green  (  0, 255,   0)
 *   layer 2 → blue   (  0,   0, 255)
 *   layer 3 → yellow (255, 255,   0)
 *
 * The fragment shader maps NDC-space UV to a 2×2 grid of layers and
 * samples mip 0 via a `array2D` view. Sampler `maxLod = 0` keeps us
 * off mip 1 (which we deliberately never upload — the storage exists,
 * but D3D12 leaves it zero-initialised).
 *
 * Expected screen layout (NDC y-up — origin at bottom-left of the
 * sampled rect, so layer 0 lands in the bottom-left quadrant):
 *   BL = layer 0 = red    TL = layer 2 = blue
 *   BR = layer 1 = green  TR = layer 3 = yellow
 *
 * A regression that drops the GL `GL_UNPACK_ROW_LENGTH` plumbing shows
 * up as 0xCC stripes (the padding sentinel) intruding into the colour
 * — easy to spot. A regression that re-introduces the transposed
 * D3D12 subresource formula maps `(layer=N, mip=0)` to `(layer=0,
 * mip=N)` which is uninitialised mip-1 storage on D3D12 → at least one
 * quadrant comes back as the zero-init colour (mostly black with
 * format-dependent garbage).
 */

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
#include "rive/renderer/ore/ore_shader_module.hpp"
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
#define ORE_ARRAY_UPLOAD_ACTIVE
#endif

#ifdef ORE_ARRAY_UPLOAD_ACTIVE
// 4×4 RGBA8, with bytesPerRow = 32 (16 valid + 16 padding) so the upload
// exercises GL_UNPACK_ROW_LENGTH on the GL backend. Padding bytes are
// filled with 0xCC so a regression that ignores bytesPerRow shows up as
// brown stripes instead of the intended solid colour.
static constexpr uint32_t kLayerWidth = 4;
static constexpr uint32_t kLayerHeight = 4;
static constexpr uint32_t kPaddedRowBytes =
    32; // 4 * 4 * 2 — double the tight row.
static constexpr uint32_t kPaddedLayerBytes = kPaddedRowBytes * kLayerHeight;

static void fillSolidLayer(uint8_t* dst, uint8_t r, uint8_t g, uint8_t b)
{
    memset(dst, 0xCC, kPaddedLayerBytes); // padding sentinel.
    for (uint32_t y = 0; y < kLayerHeight; ++y)
    {
        uint8_t* row = dst + y * kPaddedRowBytes;
        for (uint32_t x = 0; x < kLayerWidth; ++x)
        {
            row[x * 4 + 0] = r;
            row[x * 4 + 1] = g;
            row[x * 4 + 2] = b;
            row[x * 4 + 3] = 0xFF;
        }
    }
}
#endif

class OreArrayUploadGM : public GM
{
public:
    OreArrayUploadGM() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_ARRAY_UPLOAD_ACTIVE
        auto& ctx = *m_ore.oreContext;

        // Array texture: 4×4, 4 layers, 2 mips. The 2-mip count is the
        // important bit — it makes the D3D12 transposed-formula bug
        // observable.
        TextureDesc texDesc{};
        texDesc.width = kLayerWidth;
        texDesc.height = kLayerHeight;
        texDesc.depthOrArrayLayers = 4;
        texDesc.numMipmaps = 2;
        texDesc.format = TextureFormat::rgba8unorm;
        texDesc.type = TextureType::array2D;
        texDesc.label = "ore_array_upload_tex";
        auto arrTex = ctx.makeTexture(texDesc);
        if (!arrTex)
            return;

        // Array2D view covering all 4 layers, both mips.
        TextureViewDesc viewDesc{};
        viewDesc.texture = arrTex.get();
        viewDesc.dimension = TextureViewDimension::array2D;
        viewDesc.baseMipLevel = 0;
        viewDesc.mipCount = 2;
        viewDesc.baseLayer = 0;
        viewDesc.layerCount = 4;
        auto arrView = ctx.makeTextureView(viewDesc);
        if (!arrView)
            return;

        // Sampler clamped to mip 0 — mip 1 is never uploaded so we
        // mustn't sample it.
        SamplerDesc sampDesc{};
        sampDesc.minFilter = Filter::nearest;
        sampDesc.magFilter = Filter::nearest;
        sampDesc.mipmapFilter = Filter::nearest;
        sampDesc.minLod = 0.0f;
        sampDesc.maxLod = 0.0f;
        sampDesc.label = "ore_array_upload_sampler";
        auto sampler = ctx.makeSampler(sampDesc);

        auto shader = ore_gm::loadShader(ctx, ore_gm::kArray2DWitness);
        if (!shader.vsModule)
            return;

        auto canvas = renderContext->makeRenderCanvas(256, 256);
        if (!canvas)
            return;
        auto canvasView = ctx.wrapCanvasTexture(canvas.get());
        if (!canvasView)
            return;

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
        pipeDesc.colorTargets[0].format = canvasView->texture()->format();
        pipeDesc.colorCount = 1;
        pipeDesc.depthStencil.depthCompare = CompareFunction::always;
        pipeDesc.depthStencil.depthWriteEnabled = false;
        pipeDesc.bindGroupLayouts = layouts;
        pipeDesc.bindGroupLayoutCount = 3;
        pipeDesc.label = "ore_array_upload_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_array_upload] pipeline creation failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        BindGroupDesc::TexEntry texEntry{0, arrView.get()};
        BindGroupDesc texBGDesc{};
        texBGDesc.layout = layout1.get();
        texBGDesc.textures = &texEntry;
        texBGDesc.textureCount = 1;
        auto texBG = ctx.makeBindGroup(texBGDesc);

        BindGroupDesc::SampEntry sampEntry{0, sampler.get()};
        BindGroupDesc sampBGDesc{};
        sampBGDesc.layout = layout2.get();
        sampBGDesc.samplers = &sampEntry;
        sampBGDesc.samplerCount = 1;
        auto sampBG = ctx.makeBindGroup(sampBGDesc);

        m_ore.beginFrame();

        // Per-layer uploads. Each upload targets `(mipLevel=0, layer=N)`
        // with a non-tightly-packed source (bytesPerRow = 32, double
        // the tight row of 16) so the GL backend's GL_UNPACK_ROW_LENGTH
        // plumbing is exercised.
        uint8_t layerData[kPaddedLayerBytes];
        struct LayerColor
        {
            uint8_t r, g, b;
        };
        const LayerColor kColors[4] = {
            {255, 0, 0},   // 0 — red
            {0, 255, 0},   // 1 — green
            {0, 0, 255},   // 2 — blue
            {255, 255, 0}, // 3 — yellow
        };
        for (uint32_t layer = 0; layer < 4; ++layer)
        {
            fillSolidLayer(layerData,
                           kColors[layer].r,
                           kColors[layer].g,
                           kColors[layer].b);
            TextureDataDesc upload{};
            upload.data = layerData;
            upload.bytesPerRow = kPaddedRowBytes;
            upload.rowsPerImage = kLayerHeight;
            upload.mipLevel = 0;
            upload.layer = layer;
            upload.width = kLayerWidth;
            upload.height = kLayerHeight;
            upload.depth = 1;
            arrTex->upload(upload);
        }

        ColorAttachment ca{};
        ca.view = canvasView.get();
        ca.loadOp = LoadOp::clear;
        ca.storeOp = StoreOp::store;
        ca.clearColor = {0, 0, 0, 1};

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_array_upload_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass.setPipeline(pipeline.get());
        pass.setBindGroup(1, texBG.get());
        pass.setBindGroup(2, sampBG.get());
        pass.setViewport(0, 0, 256, 256);
        pass.draw(3); // big-triangle fullscreen — covers entire NDC.
        pass.finish();

        m_ore.endFrame();
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

GMREGISTER(ore_array_upload, return new OreArrayUploadGM())
