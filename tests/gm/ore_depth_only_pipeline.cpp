/*
 * Copyright 2026 Rive
 *
 * Witness GM for depth-only render pipelines (no fragment stage).
 *
 * Verifies two related guarantees per the Dawn / WebGPU model:
 *   1. POSITIVE: a pipeline with `fragmentModule == nullptr` and
 *      `colorCount == 0` is accepted, and a render pass against a
 *      depth-only attachment runs without backend errors. The
 *      rasterizer writes interpolated z to the depth target with no
 *      fragment shader.
 *   2. NEGATIVE: a pipeline with `colorCount > 0` and
 *      `fragmentModule == nullptr` is REJECTED at make time
 *      (color outputs require a fragment shader).
 *
 * Renders GREEN on success (both behaviors as expected) and RED on
 * failure (depth-only pipeline rejected, OR color-without-fragment
 * was incorrectly accepted).
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_VK) || defined(ORE_BACKEND_WGPU) ||                    \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12)
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_VK) || defined(ORE_BACKEND_WGPU) ||                    \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12)
using namespace rive::ore;
#define ORE_DEPTH_ONLY_ACTIVE
#endif

class OreDepthOnlyPipelineGM : public GM
{
public:
    OreDepthOnlyPipelineGM() : GM(128, 128) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_DEPTH_ONLY_ACTIVE
        auto& ctx = *m_ore.oreContext;

        auto shader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
        if (!shader.vsModule)
            return;

        bool ok = true;

        // Vertex layout for the depth-only pipeline. Hoisted to function
        // scope: Pipeline shallow-copies PipelineDesc (mirrors Lua's
        // `ownedVertexLayoutData`), so these must outlive the rcp below.
        // VertexAttribute = { format, offset, shaderSlot }.
        VertexAttribute attrs[] = {
            {VertexFormat::float2, 0, 0},
            {VertexFormat::float4, 8, 1},
        };
        VertexBufferLayout vbl{};
        vbl.stride = 8 + 16;
        vbl.stepMode = VertexStepMode::vertex;
        vbl.attributes = attrs;
        vbl.attributeCount = 2;

        // ── POSITIVE: depth-only pipeline (no fragment, no color). ──
        // Must succeed and be usable in a real render pass.
        rcp<ore::Pipeline> depthOnlyPipeline;
        {
            PipelineDesc pd{};
            pd.vertexModule = shader.vsModule.get();
            pd.fragmentModule = nullptr; // ← the thing under test
            pd.vertexEntryPoint = shader.vsEntryPoint;
            pd.vertexBuffers = &vbl;
            pd.vertexBufferCount = 1;
            pd.topology = PrimitiveTopology::triangleList;
            pd.colorCount = 0;
            pd.depthStencil.format = TextureFormat::depth32float;
            pd.depthStencil.depthCompare = CompareFunction::always;
            pd.depthStencil.depthWriteEnabled = true;
            pd.label = "depth_only";

            std::string err;
            depthOnlyPipeline = ctx.makePipeline(pd, &err);
            if (!depthOnlyPipeline)
            {
                fprintf(stderr,
                        "[ore_depth_only_pipeline] depth-only pipeline "
                        "REJECTED: %s\n",
                        err.c_str());
                ok = false;
            }
        }

        // ── NEGATIVE: color outputs without a fragment shader must be
        // rejected at make time.
        {
            PipelineDesc pd{};
            pd.vertexModule = shader.vsModule.get();
            pd.fragmentModule = nullptr;
            pd.vertexEntryPoint = shader.vsEntryPoint;
            pd.vertexBufferCount = 0;
            pd.topology = PrimitiveTopology::triangleList;
            pd.colorTargets[0].format = TextureFormat::rgba8unorm;
            pd.colorCount = 1;
            pd.depthStencil.depthCompare = CompareFunction::always;
            pd.depthStencil.depthWriteEnabled = false;
            pd.label = "color_without_fragment";

            std::string err;
            auto bad = ctx.makePipeline(pd, &err);
            if (bad != nullptr)
            {
                fprintf(stderr,
                        "[ore_depth_only_pipeline] color-without-fragment "
                        "INCORRECTLY accepted\n");
                ok = false;
            }
        }

        // ── If the depth-only pipeline was accepted, also exercise a
        // real depth-only render pass against it. Catches issues that
        // surface only at execution time (e.g. invalid VkRenderPass
        // compatibility, Metal pipeline state errors).
        if (depthOnlyPipeline)
        {
            TextureDesc td{};
            td.width = 64;
            td.height = 64;
            td.format = TextureFormat::depth32float;
            td.renderTarget = true;
            td.label = "depth_target";
            auto depthTex = ctx.makeTexture(td);

            TextureViewDesc tvd{};
            tvd.texture = depthTex.get();
            auto depthView = ctx.makeTextureView(tvd);

            // One triangle, three vec2 + vec4 vertices. Depth=0.5 via
            // the vertex shader's `vec4f(pos, 0.0, 1.0)` is fine — z=0
            // is also valid. We just need any draw to exercise the
            // rasterizer-only path.
            const float verts[] = {
                // x,    y,     r, g, b, a
                -0.5f,
                -0.5f,
                1,
                0,
                0,
                1,
                0.5f,
                -0.5f,
                0,
                1,
                0,
                1,
                0.0f,
                0.5f,
                0,
                0,
                1,
                1,
            };
            BufferDesc bd{};
            bd.size = sizeof(verts);
            bd.usage = BufferUsage::vertex;
            auto vbuf = ctx.makeBuffer(bd);
            vbuf->update(verts, sizeof(verts));

            m_ore.beginFrame();

            RenderPassDesc rp{};
            rp.colorCount = 0; // depth-only pass
            rp.depthStencil.view = depthView.get();
            rp.depthStencil.depthLoadOp = LoadOp::clear;
            rp.depthStencil.depthStoreOp = StoreOp::store;
            rp.depthStencil.depthClearValue = 1.0f;
            rp.label = "depth_only_pass";

            ctx.clearLastError();
            auto pass = ctx.beginRenderPass(rp);
            pass.setPipeline(depthOnlyPipeline.get());
            pass.setVertexBuffer(0, vbuf.get());
            pass.setViewport(0, 0, 64, 64);
            pass.draw(3);
            pass.finish();

            if (!ctx.lastError().empty())
            {
                fprintf(stderr,
                        "[ore_depth_only_pipeline] pass error: %s\n",
                        ctx.lastError().c_str());
                ok = false;
            }

            m_ore.endFrame();
            ore_gm::invalidateGLStateAfterOre(renderContext);
        }

        // ── Encode result as a solid color into the GM canvas.
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
        ca.clearColor = ok ? ClearColor{0.0f, 1.0f, 0.0f, 1.0f}
                           : ClearColor{1.0f, 0.0f, 0.0f, 1.0f};

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_depth_only_pipeline_result";

        auto resultPass = ctx.beginRenderPass(rpDesc);
        resultPass.setViewport(0, 0, 128, 128);
        resultPass.finish();

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

GMREGISTER(ore_depth_only_pipeline, return new OreDepthOnlyPipelineGM)
