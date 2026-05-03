/*
 * Copyright 2025 Rive
 */

// GM test for Ore cubemap textures. Renders a unique solid color to each of
// the 6 cube faces, then samples the cubemap with a shader that maps UV to
// a cube direction. The output shows a 3x2 grid of the 6 face colors.
// Verifies: TextureType::cube, per-face TextureView rendering,
// cube texture sampling, and TextureViewDimension::cube.

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
#endif

// Colors for each cube face: +X red, -X cyan, +Y green, -Y magenta,
// +Z blue, -Z yellow.

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
static const float kFaceColors[6][4] = {
    {1, 0, 0, 1}, // +X: red
    {0, 1, 1, 1}, // -X: cyan
    {0, 1, 0, 1}, // +Y: green
    {1, 0, 1, 1}, // -Y: magenta
    {0, 0, 1, 1}, // +Z: blue
    {1, 1, 0, 1}, // -Z: yellow
};
#endif

class OreCubemapGM : public GM
{
public:
    // 3x2 grid of faces, each 64x64 = 192x128 total.
    OreCubemapGM() : GM(192, 128) {}

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
        constexpr uint32_t kFaceSize = 64;

        // Create cubemap texture.
        TextureDesc cubeTexDesc{};
        cubeTexDesc.width = kFaceSize;
        cubeTexDesc.height = kFaceSize;
        cubeTexDesc.depthOrArrayLayers = 6;
        cubeTexDesc.format = TextureFormat::rgba8unorm;
        cubeTexDesc.type = TextureType::cube;
        cubeTexDesc.renderTarget = true;
        cubeTexDesc.label = "cubemap";
        auto cubeTex = ctx.makeTexture(cubeTexDesc);

        // Create per-face 2D views for rendering to each face.
        rcp<TextureView> faceViews[6];
        for (int i = 0; i < 6; ++i)
        {
            TextureViewDesc tvd{};
            tvd.texture = cubeTex.get();
            tvd.dimension = TextureViewDimension::texture2D;
            tvd.baseLayer = i;
            tvd.layerCount = 1;
            faceViews[i] = ctx.makeTextureView(tvd);
        }

        // Create a cube view for sampling all 6 faces.
        TextureViewDesc cubeViewDesc{};
        cubeViewDesc.texture = cubeTex.get();
        cubeViewDesc.dimension = TextureViewDimension::cube;
        cubeViewDesc.layerCount = 6;
        auto cubeView = ctx.makeTextureView(cubeViewDesc);

        // Fill each cube face with a unique color using clear.
        m_ore.beginFrame();

        for (int i = 0; i < 6; ++i)
        {
            RenderPassDesc rpDesc{};
            rpDesc.colorAttachments[0].view = faceViews[i].get();
            rpDesc.colorAttachments[0].loadOp = LoadOp::clear;
            rpDesc.colorAttachments[0].storeOp = StoreOp::store;
            rpDesc.colorAttachments[0].clearColor = {kFaceColors[i][0],
                                                     kFaceColors[i][1],
                                                     kFaceColors[i][2],
                                                     kFaceColors[i][3]};
            rpDesc.colorCount = 1;
            rpDesc.label = "cube_face_pass";

            auto pass = ctx.beginRenderPass(rpDesc);
            pass.finish();
        }

        // Now sample the cubemap and render a 3x2 grid showing all faces.
        // Load shader from the pre-compiled RSTB.
        auto shader = ore_gm::loadShader(ctx, ore_gm::kCubemap);
        if (!shader.vsModule)
            return;

        auto cubeLayout1 =
            ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 1);
        auto cubeLayout2 =
            ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 2);
        BindGroupLayout* cubeLayouts[] = {nullptr,
                                          cubeLayout1.get(),
                                          cubeLayout2.get()};

        PipelineDesc cubeSamplePipeDesc{};
        cubeSamplePipeDesc.vertexModule = shader.vsModule.get();
        cubeSamplePipeDesc.fragmentModule = shader.psModule.get();
        cubeSamplePipeDesc.vertexEntryPoint = shader.vsEntryPoint;
        cubeSamplePipeDesc.fragmentEntryPoint = shader.fsEntryPoint;
        cubeSamplePipeDesc.vertexBufferCount = 0;
        cubeSamplePipeDesc.topology = PrimitiveTopology::triangleList;
        cubeSamplePipeDesc.colorTargets[0].format = TextureFormat::rgba8unorm;
        cubeSamplePipeDesc.colorCount = 1;
        cubeSamplePipeDesc.depthStencil.depthCompare = CompareFunction::always;
        cubeSamplePipeDesc.depthStencil.depthWriteEnabled = false;
        cubeSamplePipeDesc.bindGroupLayouts = cubeLayouts;
        cubeSamplePipeDesc.bindGroupLayoutCount = 3;
        cubeSamplePipeDesc.label = "cube_sample_pipeline";
        auto cubeSamplePipeline = ctx.makePipeline(cubeSamplePipeDesc);

        SamplerDesc sampDesc{};
        sampDesc.minFilter = Filter::nearest;
        sampDesc.magFilter = Filter::nearest;
        sampDesc.label = "cube_sampler";
        auto sampler = ctx.makeSampler(sampDesc);

        // Render to canvas.
        auto canvas = renderContext->makeRenderCanvas(192, 128);
        if (!canvas)
            return;
        auto canvasView = ctx.wrapCanvasTexture(canvas.get());

        RenderPassDesc samplePassDesc{};
        samplePassDesc.colorAttachments[0].view = canvasView.get();
        samplePassDesc.colorAttachments[0].loadOp = LoadOp::clear;
        samplePassDesc.colorAttachments[0].storeOp = StoreOp::store;
        samplePassDesc.colorAttachments[0].clearColor = {0, 0, 0, 1};
        samplePassDesc.colorCount = 1;
        samplePassDesc.label = "cube_sample_pass";

        // Create bind groups for the texture (group 1) and sampler (group 2).
        BindGroupDesc::TexEntry texEntry{0, cubeView.get()};
        BindGroupDesc texBGDesc{};
        texBGDesc.layout = cubeLayout1.get();
        texBGDesc.textures = &texEntry;
        texBGDesc.textureCount = 1;
        auto texBG = ctx.makeBindGroup(texBGDesc);

        BindGroupDesc::SampEntry sampEntry{0, sampler.get()};
        BindGroupDesc sampBGDesc{};
        sampBGDesc.layout = cubeLayout2.get();
        sampBGDesc.samplers = &sampEntry;
        sampBGDesc.samplerCount = 1;
        auto sampBG = ctx.makeBindGroup(sampBGDesc);

        auto samplePass = ctx.beginRenderPass(samplePassDesc);
        samplePass.setPipeline(cubeSamplePipeline.get());
        samplePass.setBindGroup(1, texBG.get());
        samplePass.setBindGroup(2, sampBG.get());
        samplePass.setViewport(0, 0, 192, 128);
        samplePass.draw(3);
        samplePass.finish();

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

GMREGISTER(ore_cubemap, return new OreCubemapGM())
