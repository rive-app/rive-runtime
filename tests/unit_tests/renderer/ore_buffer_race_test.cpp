/*
 * Copyright 2026 Rive
 *
 * Multi-frame regression test for the Ore GPUBuffer per-frame update race fix
 * (PR #12971). Unlike the ore_buffer_update_between_draws GM, which is a single
 * host frame compared against a golden, this drives several real host frames
 * with a persistent buffer and asserts read-back pixels in code, so it can
 * exercise paths the GM structurally cannot:
 *
 *   - WebGPU partial-update shadow seeding, and the Metal/Vulkan/D3D12
 *     copy-forward: bind, then a PARTIAL update that orphans, must keep the
 *     untouched bytes. The witness color makes a regression visible — a draw
 *     that lost the preserved channels renders red instead of yellow.
 *   - Cross-frame backing reclaim: a backing bound in frame N is reused in a
 *     later frame; reusing one the GPU still reads would corrupt the pixel.
 *   - The D3D12 reuse-the-just-retired-backing self-copy guard (bind in one
 *     frame, update in the next), which aborts an ASan build if it regresses.
 *
 * Runs on the racy backends the fix changed (Metal, D3D12, Vulkan, WebGPU) that
 * are both compiled in and creatable headless: Metal in the macOS job, D3D12 in
 * the Windows job, Vulkan/WebGPU when their build flags are set. GL and D3D11
 * are unchanged by the fix and are intentionally not exercised here.
 */

#include "common/testing_window.hpp"
#include "gm/ore_gm_helper.hpp"
#include <catch.hpp>

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_VK) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_D3D12)
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#define ORE_BUFFER_RACE_TEST_ACTIVE
#endif

#ifdef ORE_BUFFER_RACE_TEST_ACTIVE
using namespace rive;
using namespace rive::gpu;
using namespace rive::ore;

namespace
{
struct Uniforms
{
    float r, g, b, a;
};
constexpr Uniforms kGreen = {0.0f, 1.0f, 0.0f, 1.0f};
constexpr Uniforms kZero = {0.0f, 0.0f, 0.0f, 0.0f};
constexpr int kW = 64;
constexpr int kH = 32;

// The racy backends the fix changed. The test runs on each that is compiled in
// and creatable headless, so one job covers every such backend it built. GL and
// D3D11 update in place correctly and are deliberately excluded; GL also has no
// headless path on the Linux unit-test runner.
const TestingWindow::Backend kRacyBackends[] = {
#if defined(ORE_BACKEND_METAL)
    TestingWindow::Backend::metal,
#endif
#if defined(ORE_BACKEND_D3D12)
    TestingWindow::Backend::d3d12,
#endif
#if defined(ORE_BACKEND_VK)
    TestingWindow::Backend::vk,
#endif
#if defined(ORE_BACKEND_WGPU)
    TestingWindow::Backend::dawn,
#endif
};

// A channel is "high" near 255 and "low" near 0, with slack for rgba8 rounding
// and the srcOver composite of the canvas onto the window target.
bool high(uint8_t c) { return c >= 0xf0; }
bool low(uint8_t c) { return c <= 0x10; }

void runScenario(TestingWindow* window)
{
    window->resize(kW, kH);
    auto* renderContext = window->renderContext();

    ore_gm::OreGMContext oreGM;
    REQUIRE(oreGM.ensureContext(renderContext));
    auto& ctx = *renderContext->getOreContext();

    // Persistent across every frame — that is what makes the cross-frame paths
    // reachable. Seeded green; a later partial update only rewrites red.
    BufferDesc colorDesc{};
    colorDesc.usage = BufferUsage::uniform;
    colorDesc.size = sizeof(Uniforms);
    colorDesc.data = &kGreen;
    colorDesc.label = "ore_buffer_race_color";
    auto colorBuf = ctx.makeBuffer(colorDesc);
    REQUIRE(colorBuf != nullptr);

    BufferDesc zeroDesc{};
    zeroDesc.usage = BufferUsage::uniform;
    zeroDesc.size = sizeof(Uniforms);
    zeroDesc.data = &kZero;
    zeroDesc.label = "ore_buffer_race_zero";
    auto zeroBuf = ctx.makeBuffer(zeroDesc);
    REQUIRE(zeroBuf != nullptr);

    auto shader = ore_gm::loadShader(ctx, ore_gm::kBindingWitness);
    REQUIRE(shader.vsModule);

    auto layout0 = ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 0);
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
    pipeDesc.label = "ore_buffer_race_pipeline";
    auto pipeline = ctx.makePipeline(pipeDesc);
    REQUIRE(pipeline != nullptr);

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
    bgDesc.label = "ore_buffer_race_bg";
    auto bg = ctx.makeBindGroup(bgDesc);
    REQUIRE(bg != nullptr);

    auto canvas = renderContext->makeRenderCanvas(kW, kH);
    REQUIRE(canvas != nullptr);
    auto canvasTarget = ctx.wrapCanvasTexture(canvas.get());
    REQUIRE(canvasTarget != nullptr);

    // Runs one host frame. Pass 1 draws the left half from the bound color,
    // then optionally a partial update rewrites only red and orphans, then pass
    // 2 draws the right half from the live (post-orphan) backing.
    auto runFrame = [&](bool partialUpdate) -> std::vector<uint8_t> {
        auto renderer = window->beginFrame({
            .name = "ore_buffer_race",
            .clearColor = 0xff000000,
            .doClear = true,
        });
        oreGM.beginFrame(renderContext);

        // Reset to green up front. Not bound yet this frame on the first frame;
        // on later frames this is itself an update-after-bind, exercising the
        // full-overwrite orphan (skip-copy) and cross-frame reclaim.
        colorBuf->update(&kGreen, sizeof(kGreen), 0);

        {
            ColorAttachment ca{};
            ca.view = canvasTarget.get();
            ca.loadOp = LoadOp::clear;
            ca.storeOp = StoreOp::store;
            ca.clearColor = {0, 0, 0, 1};
            RenderPassDesc rpDesc{};
            rpDesc.colorAttachments[0] = ca;
            rpDesc.colorCount = 1;
            rpDesc.label = "ore_buffer_race_pass1";
            auto pass = ctx.beginRenderPass(rpDesc);
            pass->setPipeline(pipeline.get());
            pass->setBindGroup(0, bg.get());
            pass->setViewport(0, 0, kW, kH);
            pass->setScissorRect(0, 0, kW / 2, kH);
            pass->draw(3);
            pass->finish();
        }

        if (partialUpdate)
        {
            // Rewrite only red. The orphan must keep green/blue/alpha; a
            // backend that zeroes the untouched bytes renders red on the right.
            const float red = 1.0f;
            colorBuf->update(&red, sizeof(red), 0);
        }

        {
            ColorAttachment ca{};
            ca.view = canvasTarget.get();
            ca.loadOp = LoadOp::load;
            ca.storeOp = StoreOp::store;
            RenderPassDesc rpDesc{};
            rpDesc.colorAttachments[0] = ca;
            rpDesc.colorCount = 1;
            rpDesc.label = "ore_buffer_race_pass2";
            auto pass = ctx.beginRenderPass(rpDesc);
            pass->setPipeline(pipeline.get());
            pass->setBindGroup(0, bg.get());
            pass->setViewport(0, 0, kW, kH);
            pass->setScissorRect(kW / 2, 0, kW / 2, kH);
            pass->draw(3);
            pass->finish();
        }

        oreGM.endFrame(renderContext);
        ore_gm::invalidateGLStateAfterOre(renderContext);

        renderer->drawImage(canvas->renderImage(),
                            {.filter = ImageFilter::nearest},
                            BlendMode::srcOver,
                            1);
        std::vector<uint8_t> pixels;
        window->endFrame(&pixels);
        return pixels;
    };

    auto pixelAt = [](const std::vector<uint8_t>& px, int x, int y) {
        return &px[(static_cast<size_t>(y) * kW + x) * 4];
    };

    // Phase 1: several frames of bind → partial-update-orphan → bind. The left
    // half stays green (bound before the update); the right half is yellow only
    // if the orphan preserved green. Persisting across frames also reuses prior
    // frames' backings.
    for (int frame = 0; frame < 4; ++frame)
    {
        auto px = runFrame(/*partialUpdate=*/true);
        REQUIRE(px.size() == static_cast<size_t>(kW) * kH * 4);
        const uint8_t* left = pixelAt(px, kW / 4, kH / 2);
        const uint8_t* right = pixelAt(px, 3 * kW / 4, kH / 2);

        INFO("frame " << frame << " left=" << +left[0] << "," << +left[1] << ","
                      << +left[2] << " right=" << +right[0] << "," << +right[1]
                      << "," << +right[2]);
        // Left: bound-before-update snapshot is green.
        CHECK(low(left[0]));
        CHECK(high(left[1]));
        CHECK(low(left[2]));
        // Right: post-orphan backing is yellow — green preserved through the
        // partial red rewrite. A preservation bug drops green here.
        CHECK(high(right[0]));
        CHECK(high(right[1]));
        CHECK(low(right[2]));
    }

    // Phase 2: bind in one frame with no trailing update, then update at the
    // start of the next. The orphan reuses the just-retired backing, which is
    // the D3D12 self-copy guard's path (a regression aborts under ASan). No
    // partial update, so both halves stay green; assert it still renders.
    runFrame(/*partialUpdate=*/false);
    {
        auto px = runFrame(/*partialUpdate=*/false);
        const uint8_t* left = pixelAt(px, kW / 4, kH / 2);
        const uint8_t* right = pixelAt(px, 3 * kW / 4, kH / 2);
        CHECK(low(left[0]));
        CHECK(high(left[1]));
        CHECK(low(right[0]));
        CHECK(high(right[1]));
    }
}
} // namespace

TEST_CASE("ore buffer per-frame update race", "[ore]")
{
    int ran = 0;
    for (auto backend : kRacyBackends)
    {
        auto* window = TestingWindow::Init(backend,
                                           {},
                                           TestingWindow::Visibility::headless);
        if (window == nullptr || window->renderContext() == nullptr ||
            !ore_gm::isOreBackendActive())
        {
            TestingWindow::Destroy();
            continue;
        }
        INFO("backend " << TestingWindow::BackendName(backend));
        runScenario(window);
        ++ran;
        TestingWindow::Destroy();
    }
    if (ran == 0)
        WARN("no racy Ore backend available headless; skipping");
}
#endif // ORE_BUFFER_RACE_TEST_ACTIVE
