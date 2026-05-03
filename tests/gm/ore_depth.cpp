/*
 * Copyright 2025 Rive
 */

// GM test for Ore depth testing. Renders three overlapping colored quads at
// different Z depths with depth testing enabled (lessEqual). The front quad
// should correctly occlude the ones behind it. Verifies: depth texture
// creation, depth attachment in render pass, pipeline depth compare/write,
// and correct depth ordering.

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
#include "rive/renderer/render_canvas.hpp"

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

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)

// Interleaved position (float3) + color (float4) per vertex.
struct DepthVertex
{
    float x, y, z;
    float r, g, b, a;
};

// Three overlapping quads (each as 2 triangles = 6 vertices).
// Arranged so they overlap in the center. Without depth test, draw order
// determines visibility. With depth test (lessEqual), the nearest quad wins.
//
// Quad 1 (red): z=0.2 (nearest), centered slightly left-up.
// Quad 2 (green): z=0.5 (middle), centered.
// Quad 3 (blue): z=0.8 (farthest), centered slightly right-down.
static const DepthVertex kDepthVertices[] = {
    // Quad 1 — red, z=0.2 (nearest), offset left-up
    {-0.6f, 0.7f, 0.2f, 1, 0, 0, 1},
    {0.3f, 0.7f, 0.2f, 1, 0, 0, 1},
    {-0.6f, -0.2f, 0.2f, 1, 0, 0, 1},
    {0.3f, 0.7f, 0.2f, 1, 0, 0, 1},
    {0.3f, -0.2f, 0.2f, 1, 0, 0, 1},
    {-0.6f, -0.2f, 0.2f, 1, 0, 0, 1},

    // Quad 2 — green, z=0.5 (middle), centered
    {-0.4f, 0.4f, 0.5f, 0, 1, 0, 1},
    {0.5f, 0.4f, 0.5f, 0, 1, 0, 1},
    {-0.4f, -0.5f, 0.5f, 0, 1, 0, 1},
    {0.5f, 0.4f, 0.5f, 0, 1, 0, 1},
    {0.5f, -0.5f, 0.5f, 0, 1, 0, 1},
    {-0.4f, -0.5f, 0.5f, 0, 1, 0, 1},

    // Quad 3 — blue, z=0.8 (farthest), offset right-down
    {-0.2f, 0.1f, 0.8f, 0, 0, 1, 1},
    {0.7f, 0.1f, 0.8f, 0, 0, 1, 1},
    {-0.2f, -0.8f, 0.8f, 0, 0, 1, 1},
    {0.7f, 0.1f, 0.8f, 0, 0, 1, 1},
    {0.7f, -0.8f, 0.8f, 0, 0, 1, 1},
    {-0.2f, -0.8f, 0.8f, 0, 0, 1, 1},
};

#endif // ORE_BACKEND_METAL || ORE_BACKEND_D3D11 || ORE_BACKEND_GL ||
       // ORE_BACKEND_VK

class OreDepthGM : public GM
{
public:
    OreDepthGM() : GM(256, 256) {}

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
        constexpr uint32_t kSize = 256;

        // Create a RenderCanvas to render into.
        auto canvas = renderContext->makeRenderCanvas(kSize, kSize);
        if (!canvas)
            return;
        auto colorTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!colorTarget)
            return;

        // Create depth texture.
        TextureDesc depthTexDesc{};
        depthTexDesc.width = kSize;
        depthTexDesc.height = kSize;
        depthTexDesc.format = TextureFormat::depth32float;
        depthTexDesc.renderTarget = true;
        depthTexDesc.numMipmaps = 1;
        depthTexDesc.label = "depth_target";
        auto depthTex = ctx.makeTexture(depthTexDesc);

        TextureViewDesc depthViewDesc{};
        depthViewDesc.texture = depthTex.get();
        depthViewDesc.mipCount = 1;
        depthViewDesc.layerCount = 1;
        auto depthView = ctx.makeTextureView(depthViewDesc);

        // Create vertex buffer.
        BufferDesc bufDesc{};
        bufDesc.usage = BufferUsage::vertex;
        bufDesc.size = sizeof(kDepthVertices);
        bufDesc.data = kDepthVertices;
        bufDesc.label = "ore_depth_vbo";
        auto vbo = ctx.makeBuffer(bufDesc);

        // Load shader from the pre-compiled RSTB.
        auto shader = ore_gm::loadShader(ctx, ore_gm::kDepth);
        if (!shader.vsModule)
            return;

        // Vertex layout: float3 position + float4 color, interleaved.
        VertexAttribute attrs[] = {
            {VertexFormat::float3, offsetof(DepthVertex, x), 0},
            {VertexFormat::float4, offsetof(DepthVertex, r), 1},
        };
        VertexBufferLayout vertexLayout{};
        vertexLayout.stride = sizeof(DepthVertex);
        vertexLayout.stepMode = VertexStepMode::vertex;
        vertexLayout.attributes = attrs;
        vertexLayout.attributeCount = 2;

        // Pipeline with depth testing enabled.
        PipelineDesc pipeDesc{};
        pipeDesc.vertexModule = shader.vsModule.get();
        pipeDesc.fragmentModule = shader.psModule.get();
        pipeDesc.vertexEntryPoint = shader.vsEntryPoint;
        pipeDesc.fragmentEntryPoint = shader.fsEntryPoint;
        pipeDesc.vertexBuffers = &vertexLayout;
        pipeDesc.vertexBufferCount = 1;
        pipeDesc.topology = PrimitiveTopology::triangleList;
        pipeDesc.colorTargets[0].format = TextureFormat::rgba8unorm;
        pipeDesc.colorCount = 1;
        pipeDesc.depthStencil.format = TextureFormat::depth32float;
        pipeDesc.depthStencil.depthCompare = CompareFunction::lessEqual;
        pipeDesc.depthStencil.depthWriteEnabled = true;
        pipeDesc.label = "ore_depth_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);

        // Render all three quads in draw order (red, green, blue).
        // Blue is drawn last but is farthest (z=0.8), so it should be
        // occluded in overlapping areas by red (z=0.2) and green (z=0.5).
        m_ore.beginFrame();

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0].view = colorTarget.get();
        rpDesc.colorAttachments[0].loadOp = LoadOp::clear;
        rpDesc.colorAttachments[0].storeOp = StoreOp::store;
        rpDesc.colorAttachments[0].clearColor = {0.1f, 0.1f, 0.1f, 1.0f};
        rpDesc.colorCount = 1;
        rpDesc.depthStencil.view = depthView.get();
        rpDesc.depthStencil.depthLoadOp = LoadOp::clear;
        rpDesc.depthStencil.depthStoreOp = StoreOp::store;
        rpDesc.depthStencil.depthClearValue = 1.0f;
        rpDesc.label = "ore_depth_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass.setPipeline(pipeline.get());
        pass.setVertexBuffer(0, vbo.get());
        pass.setViewport(0, 0, kSize, kSize);
        // Draw all 18 vertices (3 quads x 6 vertices each).
        pass.draw(18);
        pass.finish();

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

GMREGISTER(ore_depth, return new OreDepthGM())
