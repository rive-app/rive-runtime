/*
 * Copyright 2026 Rive
 *
 * GM test for Ore's texture → view → pipeline → render path.
 *
 * Creates a small RGBA checkerboard texture via ore::Context::makeTexture(),
 * wraps it in an ore::TextureView, builds a fullscreen-quad pipeline that
 * samples the texture, renders to a RenderCanvas, and composites into the
 * main framebuffer.
 *
 * If the image appears as a 4×4 checkerboard (red/blue squares), Ore's
 * texture and render-pipeline integration is working for this backend.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_VK) || defined(ORE_BACKEND_WGPU) ||                    \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12)
#include "rive/renderer/render_canvas.hpp"
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

// 4×4 RGBA8 checkerboard: red (0xFF0000FF) and blue (0x0000FFFF).
static void make4x4Checkerboard(uint8_t pixels[4 * 4 * 4])
{
    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            int i = (y * 4 + x) * 4;
            bool isRed = ((x + y) % 2 == 0);
            pixels[i + 0] = isRed ? 255 : 0; // R
            pixels[i + 1] = 0;               // G
            pixels[i + 2] = isRed ? 0 : 255; // B
            pixels[i + 3] = 255;             // A
        }
    }
}

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_VK) || defined(ORE_BACKEND_WGPU) ||                    \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12)
#define ORE_IMAGE_VIEW_ACTIVE
#endif

class OreImageViewGM : public GM
{
public:
    OreImageViewGM() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_IMAGE_VIEW_ACTIVE
        auto& ctx = *m_ore.oreContext;

        // ── 1. Create a 4×4 test image as an ore texture.
        uint8_t pixels[4 * 4 * 4];
        make4x4Checkerboard(pixels);

        TextureDesc imgTexDesc{};
        imgTexDesc.width = 4;
        imgTexDesc.height = 4;
        imgTexDesc.format = TextureFormat::rgba8unorm;
        imgTexDesc.type = TextureType::texture2D;
        imgTexDesc.numMipmaps = 1;
        imgTexDesc.sampleCount = 1;
        imgTexDesc.label = "ore_image_view_checkerboard";
        auto oreTex = ctx.makeTexture(imgTexDesc);
        if (!oreTex)
            return;

        TextureDataDesc uploadDesc{};
        uploadDesc.data = pixels;
        uploadDesc.width = 4;
        uploadDesc.height = 4;
        uploadDesc.bytesPerRow = 4 * 4; // rgba8unorm = 4 bytes per pixel
        uploadDesc.rowsPerImage = 4;

        // ── 2. Create a view of the texture ──
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

        // ── 3. Create sampler (nearest for crisp checkerboard) ──
        SamplerDesc sampDesc{};
        sampDesc.minFilter = Filter::nearest;
        sampDesc.magFilter = Filter::nearest;
        auto sampler = ctx.makeSampler(sampDesc);

        // ── 4. Load shader from the pre-compiled RSTB ──
        auto shader = ore_gm::loadShader(ctx, ore_gm::kImageView);
        if (!shader.vsModule)
            return;

        // ── 5. Create pipeline (no vertex buffers — fullscreen quad from
        // vertex_index) ──
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
        pipeDesc.label = "ore_image_view_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_image_view] pipeline creation failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // ── 6. Create bind groups for texture + sampler ──
        BindGroupDesc texBGDesc{};
        texBGDesc.layout = layout1.get();
        BindGroupDesc::TexEntry texEntry{};
        texEntry.slot = 0;
        texEntry.view = texView.get();
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

        // ── 7. Render into a RenderCanvas ──
        auto canvas = renderContext->makeRenderCanvas(256, 256);
        if (!canvas)
            return;

        auto canvasTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!canvasTarget)
            return;

        m_ore.beginFrame();

        // Upload must happen after beginFrame() so the command buffer is
        // in the recording state.
        oreTex->upload(uploadDesc);

        ColorAttachment ca{};
        ca.view = canvasTarget.get();
        ca.loadOp = LoadOp::clear;
        ca.storeOp = StoreOp::store;
        ca.clearColor = {0, 0, 0, 1};

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_image_view_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass.setPipeline(pipeline.get());
        pass.setBindGroup(1, texBG.get());
        pass.setBindGroup(2, sampBG.get());
        pass.setViewport(0, 0, 256, 256);
        pass.draw(6); // fullscreen quad — 2 triangles
        pass.finish();

        m_ore.endFrame();
        ore_gm::invalidateGLStateAfterOre(renderContext);

        // ── 8. Composite into main framebuffer ──
        originalRenderer->save();
        originalRenderer->drawImage(canvas->renderImage(),
                                    {.filter = ImageFilter::nearest},
                                    BlendMode::srcOver,
                                    1);
        originalRenderer->restore();
#endif // ORE_IMAGE_VIEW_ACTIVE
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_image_view, return new OreImageViewGM)
