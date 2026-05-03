/*
 * Copyright 2026 Rive
 *
 * GM test / Phase 4 witness for the post-review fix sweep
 * (`/Users/luigi/Projects/plan/Ore/Post-Review Fix Plan.md` Phase 4
 * — locks in the MSAA-resolve fixes from Phase 3 Vulkan + Metal).
 *
 * Renders the same triangle as `ore_triangle` but with `sampleCount=4`
 * MSAA, resolving into the canvas texture. Without working MSAA
 * resolve the canvas keeps the clear color (the MSAA buffer is
 * rendered but never blitted out) and the GM paints uniform grey.
 * With working resolve the triangle is anti-aliased — the diagonal
 * edges show smoothed-color pixels rather than hard transitions.
 *
 * Locks in:
 *   - Vulkan `pResolveAttachments` wiring (commit `201873659e`):
 *     pre-fix `subpass.pResolveAttachments` was always null and the
 *     `resolveTarget` was silently ignored on every Vulkan pipeline.
 *   - Metal `StoreAndMultisampleResolve` mapping (`8e1cbc9e19`):
 *     pre-fix any color attachment with `resolveTarget` set
 *     unconditionally collapsed `storeAction` to
 *     `MultisampleResolve`, which is fine for this test (we use
 *     `StoreOp::discard`) but the new branch covers the
 *     store-and-resolve case the prior code dropped.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
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
#define ORE_MSAA_RESOLVE_ACTIVE

struct MsaaVertex
{
    float x, y;
    float r, g, b, a;
};

static const MsaaVertex kMsaaVertices[] = {
    //   x      y       r     g     b     a
    {0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f},   // top — red
    {-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f}, // bottom-left — green
    {0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f},  // bottom-right — blue
};
#endif

class OreMsaaResolveGM : public GM
{
public:
    OreMsaaResolveGM() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_MSAA_RESOLVE_ACTIVE
        auto& ctx = *m_ore.oreContext;

        // Backends that don't advertise multi-sample support skip the GM —
        // no golden produced, mismatch surfaces as a missing-image diff
        // rather than a corrupt one.
        if (ctx.features().maxSamples < 4)
        {
            fprintf(stderr,
                    "[ore_msaa_resolve] skipped: maxSamples=%u < 4\n",
                    ctx.features().maxSamples);
            return;
        }

        auto canvas = renderContext->makeRenderCanvas(256, 256);
        if (!canvas)
        {
            fprintf(stderr, "[ore_msaa_resolve] makeRenderCanvas failed\n");
            return;
        }

        // Resolve target — the canvas's single-sample backing texture.
        auto resolveView = ctx.wrapCanvasTexture(canvas.get());
        if (!resolveView)
        {
            fprintf(stderr,
                    "[ore_msaa_resolve] wrapCanvasTexture failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // MSAA color attachment — separate 4× sample texture.
        TextureDesc msaaDesc{};
        msaaDesc.width = 256;
        msaaDesc.height = 256;
        msaaDesc.format = resolveView->texture()->format();
        msaaDesc.type = TextureType::texture2D;
        msaaDesc.numMipmaps = 1;
        msaaDesc.sampleCount = 4;
        msaaDesc.renderTarget = true;
        msaaDesc.label = "ore_msaa_resolve_msaa_color";
        auto msaaColor = ctx.makeTexture(msaaDesc);
        if (!msaaColor)
        {
            fprintf(stderr,
                    "[ore_msaa_resolve] msaaColor makeTexture (sampleCount=4) "
                    "failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        TextureViewDesc msaaViewDesc{};
        msaaViewDesc.texture = msaaColor.get();
        msaaViewDesc.dimension = TextureViewDimension::texture2D;
        msaaViewDesc.baseMipLevel = 0;
        msaaViewDesc.mipCount = 1;
        msaaViewDesc.baseLayer = 0;
        msaaViewDesc.layerCount = 1;
        auto msaaView = ctx.makeTextureView(msaaViewDesc);
        if (!msaaView)
        {
            fprintf(stderr,
                    "[ore_msaa_resolve] msaaView makeTextureView failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // Vertex buffer.
        BufferDesc vbDesc{};
        vbDesc.usage = BufferUsage::vertex;
        vbDesc.size = sizeof(kMsaaVertices);
        vbDesc.data = kMsaaVertices;
        vbDesc.label = "ore_msaa_resolve_vbo";
        auto vbo = ctx.makeBuffer(vbDesc);

        auto shader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
        if (!shader.vsModule)
        {
            fprintf(stderr,
                    "[ore_msaa_resolve] loadShader failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        VertexAttribute attrs[] = {
            {VertexFormat::float2, offsetof(MsaaVertex, x), 0},
            {VertexFormat::float4, offsetof(MsaaVertex, r), 1},
        };
        VertexBufferLayout vertexLayout{};
        vertexLayout.stride = sizeof(MsaaVertex);
        vertexLayout.stepMode = VertexStepMode::vertex;
        vertexLayout.attributes = attrs;
        vertexLayout.attributeCount = 2;

        PipelineDesc pipeDesc{};
        pipeDesc.vertexModule = shader.vsModule.get();
        pipeDesc.fragmentModule = shader.psModule.get();
        pipeDesc.vertexEntryPoint = shader.vsEntryPoint;
        pipeDesc.fragmentEntryPoint = shader.fsEntryPoint;
        pipeDesc.vertexBuffers = &vertexLayout;
        pipeDesc.vertexBufferCount = 1;
        pipeDesc.topology = PrimitiveTopology::triangleList;
        pipeDesc.colorTargets[0].format = msaaDesc.format;
        pipeDesc.colorCount = 1;
        pipeDesc.depthStencil.depthCompare = CompareFunction::always;
        pipeDesc.depthStencil.depthWriteEnabled = false;
        // The whole point of this GM — a 4×-MSAA pipeline whose render
        // pass declares a resolve target.
        pipeDesc.sampleCount = 4;
        pipeDesc.label = "ore_msaa_resolve_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_msaa_resolve] pipeline creation failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        m_ore.beginFrame();

        ColorAttachment ca{};
        ca.view = msaaView.get();
        ca.resolveTarget = resolveView.get();
        ca.loadOp = LoadOp::clear;
        // We don't need the MSAA buffer after resolve — discard saves
        // memory bandwidth on tilers.
        ca.storeOp = StoreOp::discard;
        ca.clearColor = {0.1f, 0.1f, 0.1f, 1.0f};

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_msaa_resolve_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass.setPipeline(pipeline.get());
        pass.setVertexBuffer(0, vbo.get());
        pass.setViewport(0, 0, 256, 256);
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
#endif
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_msaa_resolve, return new OreMsaaResolveGM())
