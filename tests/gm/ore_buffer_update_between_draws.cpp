/*
 * Copyright 2026 Rive
 *
 * GM test / regression witness for the GPUBuffer per-frame update race
 * (`/Users/luigi/Projects/plan/Ore/Ore — GPUBuffer Per-Frame Update Race
 * & Fix Plan.md`).
 *
 * Deterministic, intra-frame proxy for the cross-frame timing race: the
 * runtime memcpy's both UBO values into the buffer before the GPU runs,
 * so a backend that updates one shared GPU-visible allocation in place
 * (Metal/Vulkan/D3D12) shows the LAST value for BOTH draws.
 *
 * The GM (64x32):
 *   - write UBO color = red
 *   - pass 1: clear black, scissor the left half, draw a fullscreen
 *     triangle whose fs returns the UBO color -> left half red
 *   - write UBO color = green  (in-place overwrite — the bug trigger)
 *   - pass 2: load, scissor the right half, re-bind the SAME bind group,
 *     draw -> right half green
 *
 * Expected (golden): left red | right green.
 * On the unfixed runtime: left green | right green (both draws read the
 * final memcpy).
 *
 * Cross-backend prediction: fails (green|green) on Metal/Vulkan/D3D12,
 * passes (red|green) on D3D11 (MAP_WRITE_DISCARD renames) and GL
 * (glBufferSubData is ordered with the command stream).
 *
 * Reuses the `kBindingWitness` shader: u_low carries the color we vary,
 * u_high is left zero, so the fs output (u_low + u_high) equals the color
 * written to u_low. This avoids adding a new RSTB shader.
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
#define ORE_BUFFER_UPDATE_BETWEEN_DRAWS_ACTIVE
#endif

class OreBufferUpdateBetweenDrawsGM : public GM
{
public:
    OreBufferUpdateBetweenDrawsGM() : GM(64, 32) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_BUFFER_UPDATE_BETWEEN_DRAWS_ACTIVE
        auto& ctx = *renderContext->getOreContext();

        struct Uniforms
        {
            float r, g, b, a;
        };
        static const Uniforms kRed = {1.0f, 0.0f, 0.0f, 0.0f};
        static const Uniforms kGreen = {0.0f, 1.0f, 0.0f, 0.0f};
        static const Uniforms kZero = {0.0f, 0.0f, 0.0f, 0.0f};

        // The color UBO we overwrite between the two draws.
        BufferDesc colorDesc{};
        colorDesc.usage = BufferUsage::uniform;
        colorDesc.size = sizeof(Uniforms);
        colorDesc.data = &kRed;
        colorDesc.label = "update_between_draws_color";
        auto colorBuf = ctx.makeBuffer(colorDesc);

        // Constant zero UBO so the shader output equals the color UBO.
        BufferDesc zeroDesc{};
        zeroDesc.usage = BufferUsage::uniform;
        zeroDesc.size = sizeof(Uniforms);
        zeroDesc.data = &kZero;
        zeroDesc.label = "update_between_draws_zero";
        auto zeroBuf = ctx.makeBuffer(zeroDesc);

        auto shader = ore_gm::loadShader(ctx, ore_gm::kBindingWitness);
        if (!shader.vsModule)
            return;

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
        pipeDesc.label = "ore_buffer_update_between_draws_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);
        if (!pipeline)
        {
            fprintf(stderr,
                    "[ore_buffer_update_between_draws] pipeline creation "
                    "failed: %s\n",
                    ctx.lastError().c_str());
            return;
        }

        BindGroupDesc::UBOEntry uboEntries[2]{};
        uboEntries[0].slot = 0; // u_low, the color we vary
        uboEntries[0].buffer = colorBuf.get();
        uboEntries[0].offset = 0;
        uboEntries[0].size = sizeof(Uniforms);
        uboEntries[1].slot = 7; // u_high, constant zero
        uboEntries[1].buffer = zeroBuf.get();
        uboEntries[1].offset = 0;
        uboEntries[1].size = sizeof(Uniforms);

        BindGroupDesc bgDesc{};
        bgDesc.layout = layout0.get();
        bgDesc.ubos = uboEntries;
        bgDesc.uboCount = 2;
        bgDesc.label = "update_between_draws_bg";
        auto bg = ctx.makeBindGroup(bgDesc);
        if (!bg)
            return;

        auto canvas = renderContext->makeRenderCanvas(64, 32);
        if (!canvas)
            return;

        auto canvasTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!canvasTarget)
            return;

        m_ore.beginFrame(renderContext);

        // ── Pass 1: red into the left half. ──
        colorBuf->update(&kRed, sizeof(kRed), 0);
        {
            ColorAttachment ca{};
            ca.view = canvasTarget.get();
            ca.loadOp = LoadOp::clear;
            ca.storeOp = StoreOp::store;
            ca.clearColor = {0, 0, 0, 1};

            RenderPassDesc rpDesc{};
            rpDesc.colorAttachments[0] = ca;
            rpDesc.colorCount = 1;
            rpDesc.label = "ore_buffer_update_between_draws_pass1";

            auto pass = ctx.beginRenderPass(rpDesc);
            pass->setPipeline(pipeline.get());
            pass->setBindGroup(0, bg.get());
            pass->setViewport(0, 0, 64, 32);
            pass->setScissorRect(0, 0, 32, 32);
            pass->draw(3);
            pass->finish();
        }

        // In-place overwrite, the bug trigger.
        colorBuf->update(&kGreen, sizeof(kGreen), 0);

        // ── Pass 2: green into the right half, re-binding the SAME group. ──
        {
            ColorAttachment ca{};
            ca.view = canvasTarget.get();
            ca.loadOp = LoadOp::load;
            ca.storeOp = StoreOp::store;

            RenderPassDesc rpDesc{};
            rpDesc.colorAttachments[0] = ca;
            rpDesc.colorCount = 1;
            rpDesc.label = "ore_buffer_update_between_draws_pass2";

            auto pass = ctx.beginRenderPass(rpDesc);
            pass->setPipeline(pipeline.get());
            pass->setBindGroup(0, bg.get());
            pass->setViewport(0, 0, 64, 32);
            pass->setScissorRect(32, 0, 32, 32);
            pass->draw(3);
            pass->finish();
        }

        m_ore.endFrame(renderContext);
        ore_gm::invalidateGLStateAfterOre(renderContext);

        originalRenderer->save();
        originalRenderer->drawImage(canvas->renderImage(),
                                    {.filter = ImageFilter::nearest},
                                    BlendMode::srcOver,
                                    1);
        originalRenderer->restore();
#endif // ORE_BUFFER_UPDATE_BETWEEN_DRAWS_ACTIVE
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_buffer_update_between_draws,
           return new OreBufferUpdateBetweenDrawsGM)
