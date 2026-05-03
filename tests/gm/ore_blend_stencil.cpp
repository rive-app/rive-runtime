/*
 * Copyright 2026 Rive
 *
 * GM test / Phase 4 witness for the post-review fix sweep
 * (`/Users/luigi/Projects/plan/Ore/Post-Review Fix Plan.md` Phase 4
 * — locks the Phase 2B Pipeline descriptor plumbing for blend factors,
 * `colorWriteMask`, and `stencilFront`/`stencilBack`).
 *
 * Two-pipeline render pass against a depth-stencil attachment:
 *
 *   1. Stencil prepass — renders a centered triangle with
 *      `stencilFront/Back` configured to `compare=always, passOp=replace`,
 *      `stencilReference=1`, and `colorWriteMask=none`. Writes the
 *      triangle silhouette into the stencil buffer; the color buffer
 *      stays at the clear color.
 *   2. Stencil-masked overlay — renders a full-screen triangle with
 *      `compare=equal, passOp=keep` (`stencilReference=1` carries over),
 *      blend factors `srcAlpha`/`oneMinusSrcAlpha`, color
 *      `(0.0, 1.0, 1.0, 0.5)` cyan-half-alpha. Only the triangle
 *      silhouette passes the stencil test, so the cyan tint shows up
 *      ONLY inside the original triangle, half-blended against the
 *      dark-grey clear color (the prepass wrote no color).
 *
 * What each Phase 2B field exercises:
 *   - `desc.stencilFront / stencilBack` (compare + passOp): controls
 *     which pixels see the stencil prepass vs. the masked overlay.
 *   - `desc.stencilReadMask / stencilWriteMask`: defaults of 0xFF, but
 *     the read path uses the mask too.
 *   - `colorTargets[0].writeMask = none` on pipeline 1: prepass must
 *     not touch the color buffer.
 *   - `colorTargets[0].blendEnabled` + per-factor blend on pipeline 2:
 *     the cyan tint blends, not replaces.
 *   - `setStencilReference(1)` on the render pass: shared reference
 *     value across both pipelines.
 *
 * Pre-Phase-2B the Lua wrapper had no path to set stencil ops or
 * writeMask; this GM exercises the C++ API directly so it locks the
 * descriptor plumbing on every backend the moment the GM is registered.
 *
 * Expected: a centered teal triangle (cyan @ alpha=0.5 over dark-grey)
 * on a dark-grey background. Regressions:
 *   - dropping `writeMask=none` paints the prepass red over the whole
 *     triangle (red instead of teal);
 *   - dropping `compare=equal` lets the cyan cover the entire frame
 *     (teal everywhere, no triangle silhouette);
 *   - dropping blend shows pure cyan rgb=(0,1,1) instead of the
 *     half-blended teal.
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
#define ORE_BLEND_STENCIL_ACTIVE

struct BSVertex
{
    float x, y;
    float r, g, b, a;
};

// Centered triangle (the stencil mask).
static const BSVertex kStencilTri[] = {
    {0.0f, 0.5f, 0.8f, 0.2f, 0.2f, 1.0f},   // top — red
    {-0.5f, -0.5f, 0.8f, 0.2f, 0.2f, 1.0f}, // bottom-left — red
    {0.5f, -0.5f, 0.8f, 0.2f, 0.2f, 1.0f},  // bottom-right — red
};

// Full-screen "big triangle" trick — covers entire NDC.
// Cyan half-alpha overlay.
static const BSVertex kOverlayTri[] = {
    {-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.5f}, // covers bottom-left
    {3.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.5f},  // extends past right
    {-1.0f, 3.0f, 0.0f, 1.0f, 1.0f, 0.5f},  // extends past top
};
#endif

class OreBlendStencilGM : public GM
{
public:
    OreBlendStencilGM() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_BLEND_STENCIL_ACTIVE
        auto& ctx = *m_ore.oreContext;
        auto canvas = renderContext->makeRenderCanvas(256, 256);
        if (!canvas)
            return;
        auto colorTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!colorTarget)
            return;

        // Depth-stencil attachment for the stencil mask. We try
        // `depth24plusStencil8` first because it's WebGPU-core (always
        // available) and D3D/GL/Vulkan/macOS-Metal all map it to a
        // native format. On Metal iOS / Apple Silicon, the underlying
        // `MTLPixelFormatDepth24Unorm_Stencil8` is unsupported and
        // `makeTexture` returns null — fall back to
        // `depth32floatStencil8`, which Metal iOS does support natively.
        // The reverse path (`depth32floatStencil8` first) breaks WebGPU
        // because the format is *optional* there
        // (`FeatureName::Depth32FloatStencil8`) and Rive's
        // `RenderContextWebGPUImpl` doesn't request the feature at
        // device-creation time. Either format gives us a stencil aspect,
        // which is all the test needs.
        TextureDesc dsDesc{};
        dsDesc.width = 256;
        dsDesc.height = 256;
        dsDesc.format = TextureFormat::depth24plusStencil8;
        dsDesc.type = TextureType::texture2D;
        dsDesc.numMipmaps = 1;
        dsDesc.sampleCount = 1;
        dsDesc.renderTarget = true;
        dsDesc.label = "ore_blend_stencil_depth";
        auto dsTex = ctx.makeTexture(dsDesc);
        if (!dsTex)
        {
            dsDesc.format = TextureFormat::depth32floatStencil8;
            dsTex = ctx.makeTexture(dsDesc);
        }
        if (!dsTex)
            return;

        TextureViewDesc dsViewDesc{};
        dsViewDesc.texture = dsTex.get();
        dsViewDesc.dimension = TextureViewDimension::texture2D;
        dsViewDesc.baseMipLevel = 0;
        dsViewDesc.mipCount = 1;
        dsViewDesc.baseLayer = 0;
        dsViewDesc.layerCount = 1;
        auto dsView = ctx.makeTextureView(dsViewDesc);
        if (!dsView)
            return;

        // Vertex buffers.
        auto makeVB = [&](const BSVertex* data,
                          size_t bytes,
                          const char* label) -> rcp<Buffer> {
            BufferDesc bd{};
            bd.usage = BufferUsage::vertex;
            bd.size = static_cast<uint32_t>(bytes);
            bd.data = data;
            bd.label = label;
            return ctx.makeBuffer(bd);
        };
        auto vbStencil = makeVB(kStencilTri,
                                sizeof(kStencilTri),
                                "ore_blend_stencil_vb_stencil");
        auto vbOverlay = makeVB(kOverlayTri,
                                sizeof(kOverlayTri),
                                "ore_blend_stencil_vb_overlay");

        auto shader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
        if (!shader.vsModule)
            return;

        VertexAttribute attrs[] = {
            {VertexFormat::float2, offsetof(BSVertex, x), 0},
            {VertexFormat::float4, offsetof(BSVertex, r), 1},
        };
        VertexBufferLayout vertexLayout{};
        vertexLayout.stride = sizeof(BSVertex);
        vertexLayout.stepMode = VertexStepMode::vertex;
        vertexLayout.attributes = attrs;
        vertexLayout.attributeCount = 2;

        // Shared pipeline-desc base. Each pipeline below tweaks the
        // stencil + blend + writeMask fields.
        auto fillBase = [&](PipelineDesc& pd) {
            pd.vertexModule = shader.vsModule.get();
            pd.fragmentModule = shader.psModule.get();
            pd.vertexEntryPoint = shader.vsEntryPoint;
            pd.fragmentEntryPoint = shader.fsEntryPoint;
            pd.vertexBuffers = &vertexLayout;
            pd.vertexBufferCount = 1;
            pd.topology = PrimitiveTopology::triangleList;
            pd.colorTargets[0].format = colorTarget->texture()->format();
            pd.colorCount = 1;
            pd.depthStencil.format = dsDesc.format;
            pd.depthStencil.depthCompare = CompareFunction::always;
            pd.depthStencil.depthWriteEnabled = false;
        };

        // Pipeline 1 — stencil prepass: write 1 to stencil where the
        // triangle covers; suppress color writes via colorWriteMask=none.
        PipelineDesc prepassDesc{};
        fillBase(prepassDesc);
        prepassDesc.colorTargets[0].writeMask = ColorWriteMask::none;
        prepassDesc.stencilFront.compare = CompareFunction::always;
        prepassDesc.stencilFront.passOp = ore::StencilOp::replace;
        prepassDesc.stencilFront.failOp = ore::StencilOp::keep;
        prepassDesc.stencilFront.depthFailOp = ore::StencilOp::keep;
        prepassDesc.stencilBack = prepassDesc.stencilFront;
        prepassDesc.stencilReadMask = 0xFF;
        prepassDesc.stencilWriteMask = 0xFF;
        prepassDesc.label = "ore_blend_stencil_prepass";
        auto prepass = ctx.makePipeline(prepassDesc);
        if (!prepass)
        {
            fprintf(stderr,
                    "[ore_blend_stencil] prepass pipeline failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // Pipeline 2 — stencil-masked overlay: pass only where the
        // stencil equals 1; alpha-blend the cyan tint over whatever
        // was in the color buffer (the clear color).
        PipelineDesc overlayDesc{};
        fillBase(overlayDesc);
        overlayDesc.colorTargets[0].writeMask = ColorWriteMask::all;
        overlayDesc.colorTargets[0].blendEnabled = true;
        overlayDesc.colorTargets[0].blend.srcColor = BlendFactor::srcAlpha;
        overlayDesc.colorTargets[0].blend.dstColor =
            BlendFactor::oneMinusSrcAlpha;
        overlayDesc.colorTargets[0].blend.colorOp = BlendOp::add;
        overlayDesc.colorTargets[0].blend.srcAlpha = BlendFactor::one;
        overlayDesc.colorTargets[0].blend.dstAlpha = BlendFactor::zero;
        overlayDesc.colorTargets[0].blend.alphaOp = BlendOp::add;
        overlayDesc.stencilFront.compare = CompareFunction::equal;
        overlayDesc.stencilFront.passOp = ore::StencilOp::keep;
        overlayDesc.stencilFront.failOp = ore::StencilOp::keep;
        overlayDesc.stencilFront.depthFailOp = ore::StencilOp::keep;
        overlayDesc.stencilBack = overlayDesc.stencilFront;
        overlayDesc.stencilReadMask = 0xFF;
        overlayDesc.stencilWriteMask = 0xFF;
        overlayDesc.label = "ore_blend_stencil_overlay";
        auto overlay = ctx.makePipeline(overlayDesc);
        if (!overlay)
        {
            fprintf(stderr,
                    "[ore_blend_stencil] overlay pipeline failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        m_ore.beginFrame();

        ColorAttachment ca{};
        ca.view = colorTarget.get();
        ca.loadOp = LoadOp::clear;
        ca.storeOp = StoreOp::store;
        ca.clearColor = {0.1f, 0.1f, 0.1f, 1.0f};

        DepthStencilAttachment dsa{};
        dsa.view = dsView.get();
        dsa.depthLoadOp = LoadOp::clear;
        dsa.depthStoreOp = StoreOp::discard;
        dsa.depthClearValue = 1.0f;
        dsa.stencilLoadOp = LoadOp::clear;
        dsa.stencilStoreOp = StoreOp::discard;
        dsa.stencilClearValue = 0;

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.depthStencil = dsa;
        rpDesc.label = "ore_blend_stencil_pass";

        auto pass = ctx.beginRenderPass(rpDesc);

        // Reference value 1 carries across both pipelines — set once.
        pass.setStencilReference(1);

        // Prepass: stencil-only, mask written by the centered triangle.
        pass.setPipeline(prepass.get());
        pass.setVertexBuffer(0, vbStencil.get());
        pass.setViewport(0, 0, 256, 256);
        pass.draw(3);

        // Overlay: full-screen alpha-blended tint, masked to the
        // stencil region.
        pass.setPipeline(overlay.get());
        pass.setVertexBuffer(0, vbOverlay.get());
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

GMREGISTER(ore_blend_stencil, return new OreBlendStencilGM())
