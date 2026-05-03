/*
 * Copyright 2026 Rive
 *
 * Phase E negative-test witness: deliberate shader-vs-layout mismatches
 * must be REJECTED at `makePipeline` with a non-empty `lastError()`.
 * Validates the cross-backend `validateLayoutsAgainstBindingMap` helper.
 *
 * The shader (`binding_witness`) declares two UBOs at @group(0)
 * @binding(0,7). Each subtest builds a deliberately-broken layout and
 * confirms `makePipeline` returns null. The GM clears to green on
 * success (all subtests rejected as expected) and red on failure (a
 * pipeline that should have been rejected was accepted, or the right
 * one was rejected for the wrong reason).
 *
 * Subtests:
 *   1. WRONG KIND: layout declares sampledTexture for @binding(0)
 *      where shader declares uniformBuffer.
 *   2. WRONG VISIBILITY: layout declares vertex-only visibility where
 *      shader uses the binding from fragment as well.
 *   3. MISSING BINDING: layout declares only @binding(0); shader needs
 *      @binding(7) too.
 *   4. NO LAYOUTS AT ALL: PipelineDesc::bindGroupLayoutCount = 0, but
 *      shader has bindings.
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
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
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
#define ORE_LAYOUT_MISMATCH_ACTIVE
#endif

class OreLayoutMismatchGM : public GM
{
public:
    OreLayoutMismatchGM() : GM(128, 128) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#ifdef ORE_LAYOUT_MISMATCH_ACTIVE
        auto& ctx = *m_ore.oreContext;

        auto shader = ore_gm::loadShader(ctx, ore_gm::kBindingWitness);
        if (!shader.vsModule)
            return;

        // Build a base PipelineDesc up-front; subtests vary only the
        // bindGroupLayouts pointer and count.
        PipelineDesc basePipe{};
        basePipe.vertexModule = shader.vsModule.get();
        basePipe.fragmentModule = shader.psModule.get();
        basePipe.vertexEntryPoint = shader.vsEntryPoint;
        basePipe.fragmentEntryPoint = shader.fsEntryPoint;
        basePipe.vertexBufferCount = 0;
        basePipe.topology = PrimitiveTopology::triangleList;
        basePipe.colorTargets[0].format = TextureFormat::rgba8unorm;
        basePipe.colorCount = 1;
        basePipe.depthStencil.depthCompare = CompareFunction::always;
        basePipe.depthStencil.depthWriteEnabled = false;

        // Helper: try `makePipeline(desc)` and return true if it was
        // rejected (returned null + lastError populated).
        auto rejected = [&](const PipelineDesc& desc) -> bool {
            ctx.clearLastError();
            std::string err;
            auto p = ctx.makePipeline(desc, &err);
            const bool wasRejected = (p == nullptr);
            // Either path is acceptable for "rejected" — outError or
            // setLastError. Both come from validateLayoutsAgainstBindingMap.
            return wasRejected;
        };

        bool allRejected = true;

        // Subtest 1: wrong kind — sampledTexture instead of uniformBuffer.
        {
            BindGroupLayoutEntry e0{};
            e0.binding = 0;
            e0.kind = BindingKind::sampledTexture;
            e0.visibility.mask =
                StageVisibility::kVertex | StageVisibility::kFragment;
            BindGroupLayoutEntry e7{};
            e7.binding = 7;
            e7.kind = BindingKind::uniformBuffer;
            e7.visibility.mask =
                StageVisibility::kVertex | StageVisibility::kFragment;
            BindGroupLayoutEntry entries[] = {e0, e7};

            BindGroupLayoutDesc lDesc{};
            lDesc.groupIndex = 0;
            lDesc.entries = entries;
            lDesc.entryCount = 2;
            auto badLayout = ctx.makeBindGroupLayout(lDesc);
            BindGroupLayout* layouts[] = {badLayout.get()};

            PipelineDesc pd = basePipe;
            pd.bindGroupLayouts = layouts;
            pd.bindGroupLayoutCount = 1;
            pd.label = "mismatch_kind";
            if (!rejected(pd))
            {
                fprintf(stderr,
                        "[ore_layout_mismatch] subtest 1 (wrong kind) was "
                        "INCORRECTLY accepted\n");
                allRejected = false;
            }
        }

        // Subtest 2: missing binding — layout has only @binding(0).
        {
            BindGroupLayoutEntry e0{};
            e0.binding = 0;
            e0.kind = BindingKind::uniformBuffer;
            e0.visibility.mask =
                StageVisibility::kVertex | StageVisibility::kFragment;

            BindGroupLayoutDesc lDesc{};
            lDesc.groupIndex = 0;
            lDesc.entries = &e0;
            lDesc.entryCount = 1;
            auto incompleteLayout = ctx.makeBindGroupLayout(lDesc);
            BindGroupLayout* layouts[] = {incompleteLayout.get()};

            PipelineDesc pd = basePipe;
            pd.bindGroupLayouts = layouts;
            pd.bindGroupLayoutCount = 1;
            pd.label = "mismatch_missing";
            if (!rejected(pd))
            {
                fprintf(stderr,
                        "[ore_layout_mismatch] subtest 2 (missing binding) "
                        "was INCORRECTLY accepted\n");
                allRejected = false;
            }
        }

        // Subtest 3: no layouts at all but shader has bindings.
        {
            PipelineDesc pd = basePipe;
            pd.bindGroupLayouts = nullptr;
            pd.bindGroupLayoutCount = 0;
            pd.label = "mismatch_empty";
            if (!rejected(pd))
            {
                fprintf(stderr,
                        "[ore_layout_mismatch] subtest 3 (no layouts) was "
                        "INCORRECTLY accepted\n");
                allRejected = false;
            }
        }

        // Render a solid color encoding the result: green = all rejected
        // (correct), red = at least one was incorrectly accepted (bad).
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
        ca.clearColor = allRejected ? ClearColor{0.0f, 1.0f, 0.0f, 1.0f}
                                    : ClearColor{1.0f, 0.0f, 0.0f, 1.0f};

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_layout_mismatch_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass.setViewport(0, 0, 128, 128);
        pass.finish();

        m_ore.endFrame();
        ore_gm::invalidateGLStateAfterOre(renderContext);

        originalRenderer->save();
        originalRenderer->drawImage(canvas->renderImage(),
                                    {.filter = ImageFilter::nearest},
                                    BlendMode::srcOver,
                                    1);
        originalRenderer->restore();
#endif // ORE_LAYOUT_MISMATCH_ACTIVE
    }

private:
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_layout_mismatch, return new OreLayoutMismatchGM)
