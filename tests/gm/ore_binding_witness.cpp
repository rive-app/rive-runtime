/*
 * Copyright 2026 Rive
 *
 * GM test / regression witness for the binding-map redesign
 * (dev/rfcs/ore_bind_group_redesign.md §11.1).
 *
 * The witness WGSL shader declares two UBOs at `@group(0) @binding(0)`
 * and `@group(0) @binding(7)` — deliberately SPARSE bindings within a
 * single (group, kind) bucket. Pre-redesign, Metal's allocator would
 * reindex these to `[[buffer(0)]]` + `[[buffer(1)]]` while the runtime
 * bound them at their raw @binding values (0 and 7), leaving
 * `[[buffer(1)]]` unbound and the second UBO reading zeros.
 *
 * The GM:
 *   - uploads u_low.color = (0.3, 0.0, 0.0, 0.0)
 *   - uploads u_high.color = (0.0, 0.6, 0.0, 0.0)
 *   - renders `u_low.color.rgb + u_high.color.rgb` on a fullscreen
 *     triangle
 *
 * Correct output (post-redesign): a muddy olive (0.3, 0.6, 0, 1) across
 * the whole frame.
 * Pre-redesign bug (for historical record): dark red (0.3, 0, 0, 1)
 * because u_high reads zeros.
 *
 * Both resources are packed into a single BindGroup at groupIndex=0 so
 * the test specifically exercises multiple UBOs with non-dense bindings
 * in the same group — the exact case RFC §11.1 witnesses.
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
#define ORE_BINDING_WITNESS_ACTIVE
#endif

class OreBindingWitnessGM : public GM
{
public:
    OreBindingWitnessGM() : GM(128, 128) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_BINDING_WITNESS_ACTIVE
        auto& ctx = *m_ore.oreContext;

        // ── UBOs. Each holds a vec4 color. WGSL UBOs require 16-byte
        // alignment of the struct; vec4<f32> is already 16 bytes.
        struct Uniforms
        {
            float r, g, b, a;
        };
        static const Uniforms kLow = {0.3f, 0.0f, 0.0f, 0.0f};
        static const Uniforms kHigh = {0.0f, 0.6f, 0.0f, 0.0f};

        BufferDesc lowDesc{};
        lowDesc.usage = BufferUsage::uniform;
        lowDesc.size = sizeof(Uniforms);
        lowDesc.data = &kLow;
        lowDesc.label = "witness_ubo_low";
        auto lowBuf = ctx.makeBuffer(lowDesc);

        BufferDesc highDesc{};
        highDesc.usage = BufferUsage::uniform;
        highDesc.size = sizeof(Uniforms);
        highDesc.data = &kHigh;
        highDesc.label = "witness_ubo_high";
        auto highBuf = ctx.makeBuffer(highDesc);

        // ── Shader from the RSTB ──
        auto shader = ore_gm::loadShader(ctx, ore_gm::kBindingWitness);
        if (!shader.vsModule)
            return;

        // ── BindGroupLayout (Phase E) — derived from the shader's binding map.
        auto layout0 =
            ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 0);
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
        pipeDesc.label = "ore_binding_witness_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_binding_witness] pipeline creation failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        // ── Single BindGroup with both UBOs. Entry slots carry WGSL
        // @binding values (0 and 7) exactly as authored — the runtime
        // `mapSlot` translates to the allocator's chosen backend slots
        // at `makeBindGroup` time.
        BindGroupDesc::UBOEntry uboEntries[2]{};
        uboEntries[0].slot = 0;
        uboEntries[0].buffer = lowBuf.get();
        uboEntries[0].offset = 0;
        uboEntries[0].size = sizeof(Uniforms);
        uboEntries[1].slot = 7;
        uboEntries[1].buffer = highBuf.get();
        uboEntries[1].offset = 0;
        uboEntries[1].size = sizeof(Uniforms);

        BindGroupDesc bgDesc{};
        bgDesc.layout = layout0.get();
        bgDesc.ubos = uboEntries;
        bgDesc.uboCount = 2;
        bgDesc.label = "witness_bg";
        auto bg = ctx.makeBindGroup(bgDesc);
        if (!bg)
            return;

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
        rpDesc.label = "ore_binding_witness_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass.setPipeline(pipeline.get());
        pass.setBindGroup(0, bg.get());
        pass.setViewport(0, 0, 128, 128);
        pass.draw(3); // fullscreen triangle
        pass.finish();

        m_ore.endFrame();
        ore_gm::invalidateGLStateAfterOre(renderContext);

        // Composite into the main framebuffer.
        originalRenderer->save();
        originalRenderer->drawImage(canvas->renderImage(),
                                    {.filter = ImageFilter::nearest},
                                    BlendMode::srcOver,
                                    1);
        originalRenderer->restore();
#endif // ORE_BINDING_WITNESS_ACTIVE
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_binding_witness, return new OreBindingWitnessGM)
