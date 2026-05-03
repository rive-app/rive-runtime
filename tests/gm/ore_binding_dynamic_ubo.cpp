/*
 * Copyright 2026 Rive
 *
 * GM test / Phase 0 witness for the post-review fix sweep
 * (`/Users/luigi/Projects/plan/Ore/Post-Review Fix Plan.md` Phase 0
 * + Phase 3).
 *
 * Locks in the WebGPU contract that `dynamicOffsets[i]` is consumed in
 * BindGroupLayout-entry order (= ascending `@binding` per RFC §3.6 BGL
 * sort), NOT in the caller's `desc.ubos[]` order.
 *
 * Two dynamic UBOs at @group(0) @binding(0) and @group(0) @binding(1).
 * One buffer with two 256-byte aligned ranges:
 *
 *   range A @ offset 0   = (1, 0, 0, 0)  — should be read by u_a
 *   range B @ offset 256 = (0, 1, 0, 0)  — should be read by u_b
 *
 * The GM constructs `BindGroupDesc::ubos` in REVERSE slot order
 * (`{slot:1}` first, `{slot:0}` second) and passes
 * `dynamicOffsets = [0, 256]` in BGL/ascending-slot order. The shader
 * emits `vec4f(u_a.r, u_b.g, 0, 1)`:
 *
 *   - Correct (BGL order): a=range A, b=range B → (1, 1, 0, 1) yellow.
 *   - Wrong (insertion order, today's Metal `m_mtlBuffers` walk): a=B,
 *     b=A → (0, 0, 0, 1) black.
 *   - Wrong (Dawn validation fail post-fix on WGPU if the BGL emits
 *     dynamic entries out of ascending order): pipeline rejection.
 *
 * Expected today (PRE-FIX):
 *   - Metal: BLACK (insertion-order walk is wrong).
 *   - Vulkan / D3D11 / D3D12 / GL / WGPU: YELLOW (slot-ascending walk
 *     happens to coincide with BGL order).
 *
 * Expected POST-FIX (Phase 3 of the fix plan, Metal sort by slot at
 * BindGroup construction):
 *   - Every backend renders yellow.
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
#define ORE_BINDING_DYNAMIC_UBO_ACTIVE
#endif

// 256-byte stride is the strictest dynamic-offset alignment requirement
// across our backends (D3D12 needs 256, D3D11.1 needs 256 enforced by
// firstConstant×16, Vulkan minUniformBufferOffsetAlignment is at most 256
// on every adapter we ship to, WebGPU spec mandates 256).
static constexpr uint32_t kDynamicAlign = 256;

class OreBindingDynamicUBOGM : public GM
{
public:
    OreBindingDynamicUBOGM() : GM(128, 128) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_BINDING_DYNAMIC_UBO_ACTIVE
        auto& ctx = *m_ore.oreContext;

        // ── One buffer with two 256-byte ranges. Range A holds the
        // values u_a should read; range B the values u_b should read.
        struct Uniforms
        {
            float r, g, b, a;
        };
        std::vector<uint8_t> bufBytes(kDynamicAlign * 2, 0);
        const Uniforms kRangeA = {1.0f, 0.0f, 0.0f, 0.0f}; // u_a sees red
        const Uniforms kRangeB = {0.0f, 1.0f, 0.0f, 0.0f}; // u_b sees green
        memcpy(bufBytes.data() + 0, &kRangeA, sizeof(kRangeA));
        memcpy(bufBytes.data() + kDynamicAlign, &kRangeB, sizeof(kRangeB));

        BufferDesc bufDesc{};
        bufDesc.usage = BufferUsage::uniform;
        bufDesc.size = static_cast<uint32_t>(bufBytes.size());
        bufDesc.data = bufBytes.data();
        bufDesc.label = "dynamic_ubo_buffer";
        auto buf = ctx.makeBuffer(bufDesc);

        // ── Shader from the RSTB ──
        auto shader = ore_gm::loadShader(ctx, ore_gm::kDynamicUBOWitness);
        if (!shader.vsModule)
            return;

        // Dynamic UBO bindings declared on the layout (RFC §3.6 / Dawn).
        const uint32_t dynBindings[] = {0, 1};
        auto layout0 = ore_gm::makeLayoutFromShader(ctx,
                                                    shader.vsModule.get(),
                                                    0,
                                                    dynBindings,
                                                    2);
        BindGroupLayout* layouts[] = {layout0.get()};

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
        pipeDesc.bindGroupLayoutCount = 1;
        pipeDesc.label = "ore_binding_dynamic_ubo_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_binding_dynamic_ubo] pipeline creation failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // ── BindGroup with UBOs in REVERSE slot order. The runtime
        // must ignore this insertion order and walk the bind group in
        // BGL/ascending-slot order when consuming `dynamicOffsets[]`.
        BindGroupDesc::UBOEntry uboEntries[2]{};
        uboEntries[0].slot = 1; // u_b
        uboEntries[0].buffer = buf.get();
        uboEntries[0].offset = 0;
        uboEntries[0].size = sizeof(Uniforms);
        uboEntries[1].slot = 0; // u_a
        uboEntries[1].buffer = buf.get();
        uboEntries[1].offset = 0;
        uboEntries[1].size = sizeof(Uniforms);

        BindGroupDesc bgDesc{};
        bgDesc.layout = layout0.get();
        bgDesc.ubos = uboEntries;
        bgDesc.uboCount = 2;
        bgDesc.label = "dynamic_ubo_bg";
        auto bg = ctx.makeBindGroup(bgDesc);
        if (!bg)
            return;

        // ── Dynamic offsets in BGL/ascending-slot order:
        //   index 0 → u_a (slot 0) → range A at offset 0
        //   index 1 → u_b (slot 1) → range B at offset kDynamicAlign
        const uint32_t dynamicOffsets[2] = {0u, kDynamicAlign};

        // ── Render into a RenderCanvas ──
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
        ca.clearColor = {0, 0, 0, 1};

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_binding_dynamic_ubo_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass.setPipeline(pipeline.get());
        pass.setBindGroup(0, bg.get(), dynamicOffsets, 2);
        pass.setViewport(0, 0, 128, 128);
        pass.draw(3); // fullscreen triangle
        pass.finish();

        m_ore.endFrame();
        ore_gm::invalidateGLStateAfterOre(renderContext);

        originalRenderer->save();
        originalRenderer->drawImage(canvas->renderImage(),
                                    {.filter = ImageFilter::nearest},
                                    BlendMode::srcOver,
                                    1);
        originalRenderer->restore();
#endif // ORE_BINDING_DYNAMIC_UBO_ACTIVE
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_binding_dynamic_ubo, return new OreBindingDynamicUBOGM)
