/*
 * Copyright 2025 Rive
 */

// GM test for the Ore graphics abstraction layer. Renders a colored triangle
// using the Ore API into a RenderCanvas, then composites the result into the
// main framebuffer via drawImage(). Verifies: makeBuffer, makeShaderModule,
// makePipeline, beginRenderPass, setVertexBuffer, draw, finish,
// wrapCanvasTexture, and the full Ore→Rive composite path.

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

// Interleaved position (float2) + color (float4) per vertex.
struct Vertex
{
    float x, y;
    float r, g, b, a;
};

static const Vertex kTriangleVertices[] = {
    //  x      y       r     g     b     a
    {0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f},   // top — red
    {-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f}, // bottom-left — green
    {0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f},  // bottom-right — blue
};

#endif // ORE_BACKEND_METAL || ... || ORE_BACKEND_VK

class OreTriangleGM : public GM
{
public:
    OreTriangleGM() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xff000000; } // black

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
        auto& ctx = *m_ore.oreContext;
        // Create a RenderCanvas to render into.
        auto canvas = renderContext->makeRenderCanvas(256, 256);
        if (!canvas)
            return;

        // Wrap the canvas texture for Ore.
        auto colorTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!colorTarget)
            return;

        // Create vertex buffer.
        BufferDesc bufDesc{};
        bufDesc.usage = BufferUsage::vertex;
        bufDesc.size = sizeof(kTriangleVertices);
        bufDesc.data = kTriangleVertices;
        bufDesc.label = "ore_triangle_vbo";
        auto vbo = ctx.makeBuffer(bufDesc);

        // Load shader from the pre-compiled RSTB.
        auto shader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
        if (!shader.vsModule)
            return;

        // Vertex layout: float2 position + float4 color, interleaved.
        VertexAttribute attrs[] = {
            {VertexFormat::float2, offsetof(Vertex, x), 0}, // position
            {VertexFormat::float4, offsetof(Vertex, r), 1}, // color
        };
        VertexBufferLayout vertexLayout{};
        vertexLayout.stride = sizeof(Vertex);
        vertexLayout.stepMode = VertexStepMode::vertex;
        vertexLayout.attributes = attrs;
        vertexLayout.attributeCount = 2;

        // Pipeline.
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
        // No depth/stencil for this simple test.
        pipeDesc.depthStencil.depthCompare = CompareFunction::always;
        pipeDesc.depthStencil.depthWriteEnabled = false;
        pipeDesc.label = "ore_triangle_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);

        // Render the triangle via Ore.
        m_ore.beginFrame();

        ColorAttachment ca{};
        ca.view = colorTarget.get();
        ca.loadOp = LoadOp::clear;
        ca.storeOp = StoreOp::store;
        ca.clearColor = {0.1f, 0.1f, 0.1f, 1.0f}; // dark grey

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_triangle_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass.setPipeline(pipeline.get());
        pass.setVertexBuffer(0, vbo.get());
        pass.setViewport(0, 0, 256, 256);
        pass.draw(3);
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

GMREGISTER(ore_triangle, return new OreTriangleGM())
