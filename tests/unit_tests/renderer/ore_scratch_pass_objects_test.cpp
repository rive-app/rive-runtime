/*
 * Copyright 2026 Rive
 *
 * The GL backend lends every render pass one persistent scratch FBO/VAO pair
 * instead of minting and deleting a pair per pass (WebGL never recycles a GL
 * name, so a per-pass pair ratchets the browser's tables for the life of the
 * page). Two things have to hold for that to be invisible:
 *
 *   1. A reused FBO must behave like a freshly minted one. `finish()` records
 *      what the borrower left attached and `acquireScratchFBO()` strips it, so
 *      a pass with fewer attachments than the last cannot inherit them. Here a
 *      two-color-plus-depth pass runs first and a single-color pass second,
 *      with the first pass's attachments deliberately SMALLER than the second's
 *      target: an FBO that still named them would clamp the second pass's
 *      render area to their intersection, so the primed color outside 32x32
 *      would survive the clear and the draw.
 *
 *   2. A pass that opens while the pair is already lent must mint and own its
 *      own — the pre-change behavior — rather than alias one FBO across two
 *      live passes. `Context::beginRenderPass` does not force the previous pass
 *      closed (`m_activeRenderPass` is never set, so `finishActiveRenderPass()`
 *      is inert), so nesting is reachable and both passes must land their own
 *      pixels.
 *
 * GL only: the scratch objects are GL names and no other backend has them. GL
 * is reachable headless through ANGLE's EGL on Apple and a real GLFW window
 * elsewhere, same as ore_split_stage_test.
 */

#include "common/testing_window.hpp"
#include "gm/ore_gm_helper.hpp"
#include <catch.hpp>

#if defined(ORE_BACKEND_GL)
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#ifdef __APPLE__
#include <dlfcn.h>
#endif
#define ORE_SCRATCH_PASS_OBJECTS_TEST_ACTIVE
#endif

#ifdef ORE_SCRATCH_PASS_OBJECTS_TEST_ACTIVE
using namespace rive;
using namespace rive::gpu;
using namespace rive::ore;

namespace
{
// Only GL has scratch pass objects. Apple's GL is ANGLE (desktop GL caps at
// 4.1 and the GLFW path asks for 4.2), everywhere else it is a real GL window.
const TestingWindow::Backend kGLBackends[] = {
#if defined(__APPLE__)
    TestingWindow::Backend::angle,
#else
    TestingWindow::Backend::gl,
#endif
};

// `TestingWindow::Init` aborts on a backend it cannot bring up rather than
// declining it, which would take every test in the binary with it. So check
// what each GL flavor needs before asking for it. Mirrors
// ore_split_stage_test.cpp.
bool backendCanStart(TestingWindow::Backend backend)
{
    if (backend == TestingWindow::Backend::gl)
    {
        return getenv("DISPLAY") != nullptr ||
               getenv("WAYLAND_DISPLAY") != nullptr;
    }
#ifdef __APPLE__
    if (backend == TestingWindow::Backend::angle)
    {
        // Held for the process; MakeEGL dlopens it again anyway.
        return dlopen("libEGL.dylib", RTLD_LAZY) != nullptr;
    }
#endif
    return true;
}

// A channel is "high" near 255 and "low" near 0, with slack for rgba8 rounding
// and the srcOver composite of the canvas onto the window target.
bool high(uint8_t c) { return c >= 0xf0; }
bool low(uint8_t c) { return c <= 0x10; }

// Three vertices that cover the whole viewport in one solid color, so a pass's
// output is a single value to assert rather than a gradient to sample.
struct SolidTriangle
{
    constexpr SolidTriangle(float r, float g, float b) :
        v{{-1.f, -1.f, r, g, b, 1.f},
          {3.f, -1.f, r, g, b, 1.f},
          {-1.f, 3.f, r, g, b, 1.f}}
    {}
    ore_gm::TriVertex v[3];
};

rcp<Buffer> makeSolidVBO(Context& ctx,
                         const SolidTriangle& tri,
                         const char* label)
{
    BufferDesc desc{};
    desc.usage = BufferUsage::vertex;
    desc.size = sizeof(tri.v);
    desc.data = tri.v;
    desc.label = label;
    return ctx.makeBuffer(desc);
}

// Composite one canvas into a freshly cleared window and read the window back.
std::vector<uint8_t> readCanvas(TestingWindow* window,
                                RenderCanvas* canvas,
                                const char* name)
{
    auto renderer = window->beginFrame({
        .name = name,
        .clearColor = 0xff000000,
        .doClear = true,
    });
    renderer->drawImage(canvas->renderImage(),
                        {.filter = ImageFilter::nearest},
                        BlendMode::srcOver,
                        1);
    std::vector<uint8_t> pixels;
    window->endFrame(&pixels);
    return pixels;
}

// ── Scenario 1: a reused FBO is handed over scrubbed ─────────────────────────
void runScrubScenario(TestingWindow* window)
{
    // Pass A's attachments are deliberately smaller than pass B's target: a
    // leftover attachment clamps the render area, which a readback can see.
    constexpr int kBig = 64;
    constexpr int kSmall = 32;

    window->resize(kBig, kBig);
    auto* renderContext = window->renderContext();
    ore_gm::OreGMContext oreGM;
    REQUIRE(oreGM.ensureContext(renderContext));
    auto& ctx = *renderContext->getOreContext();

    auto shader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
    REQUIRE(shader.vsModule);
    ore_gm::TrianglePipeline pipe(shader,
                                  TextureFormat::rgba8unorm,
                                  "ore_scratch_scrub_pipeline");
    std::string pipelineError;
    auto pipeline = ctx.makePipeline(pipe.desc, &pipelineError);
    INFO("makePipeline: " << pipelineError);
    REQUIRE(pipeline != nullptr);

    SolidTriangle green(0.f, 1.f, 0.f);
    auto greenVBO = makeSolidVBO(ctx, green, "ore_scratch_scrub_green");
    REQUIRE(greenVBO != nullptr);

    // Pass B's target.
    auto canvasB = renderContext->makeRenderCanvas(kBig, kBig);
    REQUIRE(canvasB != nullptr);
    auto viewB = ctx.wrapCanvasTexture(canvasB.get());
    REQUIRE(viewB != nullptr);

    // Pass A's attachments: two colors plus depth, all kSmall.
    TextureDesc colorDesc{};
    colorDesc.width = kSmall;
    colorDesc.height = kSmall;
    colorDesc.format = TextureFormat::rgba8unorm;
    colorDesc.renderTarget = true;
    colorDesc.numMipmaps = 1;
    colorDesc.label = "ore_scratch_scrub_a0";
    auto texA0 = ctx.makeTexture(colorDesc);
    REQUIRE(texA0 != nullptr);
    TextureViewDesc a0ViewDesc{};
    a0ViewDesc.texture = texA0.get();
    a0ViewDesc.mipCount = 1;
    a0ViewDesc.layerCount = 1;
    auto viewA0 = ctx.makeTextureView(a0ViewDesc);
    REQUIRE(viewA0 != nullptr);

    // A canvas, so the second color attachment can be composited and read.
    auto canvasA1 = renderContext->makeRenderCanvas(kSmall, kSmall);
    REQUIRE(canvasA1 != nullptr);
    auto viewA1 = ctx.wrapCanvasTexture(canvasA1.get());
    REQUIRE(viewA1 != nullptr);

    TextureDesc depthDesc{};
    depthDesc.width = kSmall;
    depthDesc.height = kSmall;
    depthDesc.format = TextureFormat::depth32float;
    depthDesc.renderTarget = true;
    depthDesc.numMipmaps = 1;
    depthDesc.label = "ore_scratch_scrub_depth";
    auto depthTex = ctx.makeTexture(depthDesc);
    REQUIRE(depthTex != nullptr);
    TextureViewDesc depthViewDesc{};
    depthViewDesc.texture = depthTex.get();
    depthViewDesc.mipCount = 1;
    depthViewDesc.layerCount = 1;
    auto depthView = ctx.makeTextureView(depthViewDesc);
    REQUIRE(depthView != nullptr);

    // Ore renders inside a host frame, and B's target is composited into the
    // same one so the whole scenario is a single frame plus one read frame.
    auto renderer = window->beginFrame({
        .name = "ore_scratch_scrub",
        .clearColor = 0xff000000,
        .doClear = true,
    });
    oreGM.beginFrame(renderContext);

    // Prime pass: paints B's target red, so anything pass B fails to cover
    // reads back as red instead of as an undefined texture.
    {
        RenderPassDesc desc{};
        desc.colorAttachments[0].view = viewB.get();
        desc.colorAttachments[0].loadOp = LoadOp::clear;
        desc.colorAttachments[0].storeOp = StoreOp::store;
        desc.colorAttachments[0].clearColor = {1.f, 0.f, 0.f, 1.f};
        desc.colorCount = 1;
        desc.label = "ore_scratch_scrub_prime";
        auto prime = ctx.beginRenderPass(desc);
        REQUIRE(prime != nullptr);
        prime->finish();
    }

    // Pass A: two colors and a depth, all smaller than B's target. Clear only
    // — the point is what it leaves attached, and a clear is the one thing
    // whose result is exactly assertable afterwards.
    {
        RenderPassDesc desc{};
        desc.colorAttachments[0].view = viewA0.get();
        desc.colorAttachments[0].loadOp = LoadOp::clear;
        desc.colorAttachments[0].storeOp = StoreOp::store;
        desc.colorAttachments[0].clearColor = {0.f, 0.f, 1.f, 1.f};
        desc.colorAttachments[1].view = viewA1.get();
        desc.colorAttachments[1].loadOp = LoadOp::clear;
        desc.colorAttachments[1].storeOp = StoreOp::store;
        desc.colorAttachments[1].clearColor = {1.f, 1.f, 0.f, 1.f}; // yellow
        desc.colorCount = 2;
        desc.depthStencil.view = depthView.get();
        desc.depthStencil.depthLoadOp = LoadOp::clear;
        desc.depthStencil.depthStoreOp = StoreOp::store;
        desc.depthStencil.depthClearValue = 1.f;
        desc.label = "ore_scratch_scrub_passA";
        auto passA = ctx.beginRenderPass(desc);
        REQUIRE(passA != nullptr);
        passA->finish();
    }

    // Pass B: one color, no depth, on the borrowed-and-scrubbed FBO.
    {
        RenderPassDesc desc{};
        desc.colorAttachments[0].view = viewB.get();
        desc.colorAttachments[0].loadOp = LoadOp::clear;
        desc.colorAttachments[0].storeOp = StoreOp::store;
        desc.colorAttachments[0].clearColor = {0.f, 0.f, 1.f, 1.f};
        desc.colorCount = 1;
        desc.label = "ore_scratch_scrub_passB";
        auto passB = ctx.beginRenderPass(desc);
        REQUIRE(passB != nullptr);
        passB->setPipeline(pipeline.get());
        passB->setVertexBuffer(0, greenVBO.get());
        passB->setViewport(0, 0, kBig, kBig);
        passB->draw(3);
        passB->finish();
    }

    oreGM.endFrame(renderContext);
    ore_gm::invalidateGLStateAfterOre(renderContext);

    // B covered its whole target: green everywhere. A stale 32x32 attachment
    // would have clipped both the clear and the draw, leaving the prime's red.
    {
        renderer->drawImage(canvasB->renderImage(),
                            {.filter = ImageFilter::nearest},
                            BlendMode::srcOver,
                            1);
        std::vector<uint8_t> px;
        window->endFrame(&px);
        REQUIRE(px.size() == static_cast<size_t>(kBig) * kBig * 4);
        for (int y = 1; y < kBig - 1; ++y)
        {
            for (int x = 1; x < kBig - 1; ++x)
            {
                const uint8_t* p = &px[(static_cast<size_t>(y) * kBig + x) * 4];
                INFO("pass B pixel " << x << "," << y << " = " << +p[0] << ","
                                     << +p[1] << "," << +p[2]);
                REQUIRE(low(p[0]));
                REQUIRE(high(p[1]));
                REQUIRE(low(p[2]));
            }
        }
    }

    // Pass A's second color texture still holds its clear. Nothing pass B drew
    // reached it, and the detach did not lose it either. The canvas is smaller
    // than the window, so scan for yellow rather than assuming where it lands.
    {
        auto px =
            readCanvas(window, canvasA1.get(), "ore_scratch_scrub_read_a1");
        REQUIRE(px.size() == static_cast<size_t>(kBig) * kBig * 4);
        int yellowCount = 0;
        int greenCount = 0;
        for (size_t i = 0; i + 3 < px.size(); i += 4)
        {
            const uint8_t* p = &px[i];
            if (high(p[0]) && high(p[1]) && low(p[2]))
                ++yellowCount;
            // What pass B drew, and what a shared attachment would leak in.
            if (low(p[0]) && high(p[1]) && low(p[2]))
                ++greenCount;
        }
        INFO("yellow=" << yellowCount << " green=" << greenCount);
        CHECK(greenCount == 0);
        CHECK(yellowCount >= kSmall * kSmall / 4);
    }
}

// ── Scenario 2: a pass that opens on a lent pair mints its own ───────────────
void runOverlapScenario(TestingWindow* window)
{
    constexpr int kSize = 64;

    window->resize(kSize, kSize);
    auto* renderContext = window->renderContext();
    ore_gm::OreGMContext oreGM;
    REQUIRE(oreGM.ensureContext(renderContext));
    auto& ctx = *renderContext->getOreContext();

    auto shader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
    REQUIRE(shader.vsModule);
    ore_gm::TrianglePipeline pipe(shader,
                                  TextureFormat::rgba8unorm,
                                  "ore_scratch_overlap_pipeline");
    std::string pipelineError;
    auto pipeline = ctx.makePipeline(pipe.desc, &pipelineError);
    INFO("makePipeline: " << pipelineError);
    REQUIRE(pipeline != nullptr);

    SolidTriangle green(0.f, 1.f, 0.f);
    SolidTriangle magenta(1.f, 0.f, 1.f);
    auto greenVBO = makeSolidVBO(ctx, green, "ore_scratch_overlap_green");
    auto magentaVBO = makeSolidVBO(ctx, magenta, "ore_scratch_overlap_magenta");
    REQUIRE(greenVBO != nullptr);
    REQUIRE(magentaVBO != nullptr);

    auto canvasOuter = renderContext->makeRenderCanvas(kSize, kSize);
    auto canvasInner = renderContext->makeRenderCanvas(kSize, kSize);
    REQUIRE(canvasOuter != nullptr);
    REQUIRE(canvasInner != nullptr);
    auto viewOuter = ctx.wrapCanvasTexture(canvasOuter.get());
    auto viewInner = ctx.wrapCanvasTexture(canvasInner.get());
    REQUIRE(viewOuter != nullptr);
    REQUIRE(viewInner != nullptr);

    auto makeDesc = [](TextureView* view, const char* label) {
        RenderPassDesc desc{};
        desc.colorAttachments[0].view = view;
        desc.colorAttachments[0].loadOp = LoadOp::clear;
        desc.colorAttachments[0].storeOp = StoreOp::store;
        // Blue survives anywhere a draw failed to land.
        desc.colorAttachments[0].clearColor = {0.f, 0.f, 1.f, 1.f};
        desc.colorCount = 1;
        desc.label = label;
        return desc;
    };

    // Ore renders inside a host frame; the two canvases are read back in
    // frames of their own afterwards.
    auto renderer = window->beginFrame({
        .name = "ore_scratch_overlap",
        .clearColor = 0xff000000,
        .doClear = true,
    });
    oreGM.beginFrame(renderContext);

    // Outer takes the scratch pair; inner opens on top of it and has to mint
    // its own FBO and VAO.
    auto outerDesc = makeDesc(viewOuter.get(), "ore_scratch_overlap_outer");
    auto passOuter = ctx.beginRenderPass(outerDesc);
    REQUIRE(passOuter != nullptr);

    auto innerDesc = makeDesc(viewInner.get(), "ore_scratch_overlap_inner");
    auto passInner = ctx.beginRenderPass(innerDesc);
    REQUIRE(passInner != nullptr);

    passInner->setPipeline(pipeline.get());
    passInner->setVertexBuffer(0, magentaVBO.get());
    passInner->setViewport(0, 0, kSize, kSize);
    passInner->draw(3);
    // Restores the outer pass's FBO and VAO on the way out, which is what lets
    // the outer pass keep drawing.
    passInner->finish();

    // Everything below the pass abstraction is global GL state, so the outer
    // pass re-states its program, vertex buffer and viewport rather than
    // assuming the nested pass left them alone.
    passOuter->setPipeline(pipeline.get());
    passOuter->setVertexBuffer(0, greenVBO.get());
    passOuter->setViewport(0, 0, kSize, kSize);
    passOuter->draw(3);
    passOuter->finish();

    oreGM.endFrame(renderContext);
    ore_gm::invalidateGLStateAfterOre(renderContext);
    window->endFrame();

    auto checkSolid = [&](RenderCanvas* canvas,
                          const char* name,
                          bool wantR,
                          bool wantG,
                          bool wantB) {
        auto px = readCanvas(window, canvas, name);
        REQUIRE(px.size() == static_cast<size_t>(kSize) * kSize * 4);
        for (int y = 1; y < kSize - 1; ++y)
        {
            for (int x = 1; x < kSize - 1; ++x)
            {
                const uint8_t* p =
                    &px[(static_cast<size_t>(y) * kSize + x) * 4];
                INFO(name << " pixel " << x << "," << y << " = " << +p[0] << ","
                          << +p[1] << "," << +p[2]);
                REQUIRE((wantR ? high(p[0]) : low(p[0])));
                REQUIRE((wantG ? high(p[1]) : low(p[1])));
                REQUIRE((wantB ? high(p[2]) : low(p[2])));
            }
        }
    };

    checkSolid(canvasOuter.get(),
               "ore_scratch_overlap_read_outer",
               false,
               true,
               false); // green
    checkSolid(canvasInner.get(),
               "ore_scratch_overlap_read_inner",
               true,
               false,
               true); // magenta
}

// Brings a GL window up, runs `scenario` on it, and tears it down. Returns
// false when no GL flavor could be started here.
bool runOnGL(void (*scenario)(TestingWindow*))
{
    bool ran = false;
    for (auto backend : kGLBackends)
    {
        if (!backendCanStart(backend))
            continue;
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
        scenario(window);
        ran = true;
        TestingWindow::Destroy();
    }
    return ran;
}
} // namespace

TEST_CASE("ore GL hands a reused render pass FBO over scrubbed", "[ore][gl]")
{
    if (!runOnGL(&runScrubScenario))
        WARN("no Ore GL backend available headless; skipping");
}

TEST_CASE("ore GL mints its own pass objects when the scratch pair is lent",
          "[ore][gl]")
{
    if (!runOnGL(&runOverlapScenario))
        WARN("no Ore GL backend available headless; skipping");
}
#endif // ORE_SCRATCH_PASS_OBJECTS_TEST_ACTIVE
