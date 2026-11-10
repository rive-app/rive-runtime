/*
 * Copyright 2026 Rive
 *
 * Layer masks, bitmap caches and artboard paints composite through 2D
 * canvases, so a GPU canvas drawn inside one must show this frame's content.
 */

#include "common/testing_window.hpp"
#include "gm/ore_gm_helper.hpp"
#include <catch.hpp>

#if ORE_GM_HAS_BACKEND && defined(RIVE_CANVAS)
#include "common/testing_window_deferred_sink.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/render_canvas.hpp"
#include <cstdlib>
#include <memory>
#include <vector>
#ifdef __APPLE__
#include <dlfcn.h>
#endif

using namespace rive;

namespace
{
constexpr uint32_t kCanvasSize = 64;
constexpr ore::ClearColor kGreen = {0.f, 1.f, 0.f, 1.f};

const TestingWindow::Backend kBackends[] = {
#if defined(__APPLE__)
    TestingWindow::Backend::metal,
    TestingWindow::Backend::angle,
#elif defined(_WIN32)
    TestingWindow::Backend::d3d,
#else
    TestingWindow::Backend::gl,
#endif
};

// Init aborts on a backend it cannot bring up, which would take every test in
// the binary with it.
bool backendCanStart(TestingWindow::Backend backend)
{
  if (backend == TestingWindow::Backend::gl)
  {
    return getenv("DISPLAY") != nullptr || getenv("WAYLAND_DISPLAY") != nullptr;
  }
#ifdef __APPLE__
  if (backend == TestingWindow::Backend::angle)
  {
    return dlopen("libEGL.dylib", RTLD_LAZY) != nullptr;
  }
#endif
  return true;
}

void recordClear(ore::Context& ore, gpu::RenderCanvas* canvas)
{
  rcp<ore::TextureView> view = ore.wrapCanvasTexture(canvas);
  REQUIRE(view != nullptr);
  ore::RenderPassDesc desc{};
  desc.colorAttachments[0].view = view.get();
  desc.colorAttachments[0].loadOp = ore::LoadOp::clear;
  desc.colorAttachments[0].storeOp = ore::StoreOp::store;
  desc.colorAttachments[0].clearColor = kGreen;
  desc.colorCount = 1;
  auto pass = ore.beginRenderPass(desc);
  REQUIRE(pass != nullptr);
  pass->setViewport(0, 0, kCanvasSize, kCanvasSize);
  pass->finish();
}

// Draws `image` over the whole window, so the center pixel reads the same
// whichever way round the readback orders rows.
void drawFullWindow(TestingWindow* window, Renderer* r, RenderImage* image)
{
  r->save();
  r->transform(Mat2D::fromScale(
      static_cast<float>(window->width()) / kCanvasSize,
      static_cast<float>(window->height()) / kCanvasSize));
  r->drawImage(image, ImageSampler::LinearClamp(), BlendMode::srcOver, 1.f);
  r->restore();
}

std::vector<uint8_t> centerPixel(
    TestingWindow* window,
    const std::vector<uint8_t>& pixels)
{
  const size_t x = window->width() / 2;
  const size_t y = window->height() / 2;
  const size_t i = (y * window->width() + x) * 4;
  REQUIRE(i + 3 < pixels.size());
  return {pixels[i], pixels[i + 1], pixels[i + 2]};
}

enum class Route
{
  // Straight to the screen, which replays after Ore anyway, so this proves
  // the harness rather than the ordering.
  screen,
  canvas,
  // Over opaque blue at half opacity, so a canvas flushed right after an
  // early Ore frame shows whether it still blends.
  blendedCanvas,
};

void drawGpuCanvas(TestingWindow* window, Route route)
{
  window->resize(kCanvasSize, kCanvasSize);
  auto* rc = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(rc));

  cmd::DeferredSession session(ore::ReplayCaps::from(*rc->getOreContext()));
  session.bindRenderContext(rc);

  // Ore writes the GPU canvas.
  rcp<gpu::RenderCanvas> gpuCanvas = session.makeContentCanvas(
      kCanvasSize,
      kCanvasSize);
  REQUIRE(gpuCanvas != nullptr);
  recordClear(session.oreContext(), gpuCanvas.get());
  session.recordOreReplayMarker();

  // A 2D canvas samples it, the way a layer mask's content canvas would
  // sample a script's GPU canvas drawn inside the masked layer.
  rcp<gpu::RenderCanvas> shown = gpuCanvas;
  const bool blended = route == Route::blendedCanvas;
  if (route != Route::screen)
  {
    shown = session.makeContentCanvas(kCanvasSize, kCanvasSize);
    REQUIRE(shown != nullptr);
    Renderer* content = session.beginCanvasContent(shown.get(), 0);
    REQUIRE(content != nullptr);
    if (blended)
    {
      Factory& factory = session;
      rcp<RenderPath> rect = factory.makeRenderPath(
          AABB(0, 0, kCanvasSize, kCanvasSize));
      rcp<RenderPaint> blue = session.makeRenderPaint();
      blue->color(0xff0000ff);
      content->drawPath(rect.get(), blue.get());
    }
    content->drawImage(
        session.contentCanvasImage(gpuCanvas.get()).get(),
        ImageSampler::LinearClamp(),
        BlendMode::srcOver,
        blended ? .5f : 1.f);
    session.endCanvasContent(shown.get());
  }

  window->beginFrame({.clearColor = 0xff000000});
  auto mainDesc = rc->frameDescriptor();
  drawFullWindow(
      window,
      session.screenRenderer(),
      session.contentCanvasImage(shown.get()).get());

  cmd::DeferredFrame frame = cmd::snapshotFrame(session);
  session.resetFrame();
  rive_tests::TestingWindowFrameSink sink(window, rc, mainDesc);
  cmd::DeferredReplayer replayer;
  replayer.replayFrame(frame, sink);

  std::vector<uint8_t> pixels;
  window->endFrame(&pixels);
  std::vector<uint8_t> center = centerPixel(window, pixels);
  INFO(
      "center rgb " << int(center[0]) << " " << int(center[1]) << " "
                    << int(center[2]));
  CHECK(center[0] <= 0x10);
  if (blended)
  {
    CHECK(std::abs(center[1] - 0x80) <= 2);
    CHECK(std::abs(center[2] - 0x80) <= 2);
  }
  else
  {
    CHECK(center[1] >= 0xf0);
    CHECK(center[2] <= 0x10);
  }
}
// Brings up each backend that can start here and runs `scenario` on it.
template <typename Scenario> void runOnOreBackends(Scenario scenario)
{
  bool ran = false;
  for (auto backend : kBackends)
  {
    if (!backendCanStart(backend))
    {
      continue;
    }
    auto* window = TestingWindow::Init(
        backend,
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
  if (!ran)
  {
    WARN("no ore backend could start here");
  }
}
} // namespace

TEST_CASE(
    "the screen sees what ore wrote into a gpu canvas the same frame",
    "[deferred][ore][canvas-order]")
{
  runOnOreBackends([](TestingWindow* w) { drawGpuCanvas(w, Route::screen); });
}

TEST_CASE(
    "a 2d canvas sees what ore wrote into a gpu canvas the same frame",
    "[deferred][ore][canvas-order]")
{
  runOnOreBackends([](TestingWindow* w) { drawGpuCanvas(w, Route::canvas); });
}

TEST_CASE(
    "a canvas flushed after an early ore frame still blends",
    "[deferred][ore][canvas-order]")
{
  runOnOreBackends(
      [](TestingWindow* w) { drawGpuCanvas(w, Route::blendedCanvas); });
}
#endif
