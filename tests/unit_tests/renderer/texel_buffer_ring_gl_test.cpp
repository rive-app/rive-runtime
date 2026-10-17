/*
 * Copyright 2026 Rive
 *
 * The GLES 3.0 storage-buffer polyfill (TexelBufferRingWebGL, which explains
 * why) uploads path, paint, paintAux and contour data into textures on every
 * flush. With avoidTexSubImage2DOnInFlightTextures, the textures rotate
 * through a ring of gpu::kBufferRingSize, so a flush never rewrites the texture
 * a still-executing flush is reading. Without it, every flush uses one texture.
 *
 * Needs a GLES context whose driver lacks vertex-stage shader storage blocks,
 * because that is what selects the polyfill. Headless that means ANGLE on
 * Apple (ANGLE lands beside the test binary in CI; locally copy libEGL and
 * libGLESv2 next to it) or a device on Android. Anywhere else, or on a driver
 * with real vertex SSBOs, the test skips itself.
 */

#include "common/testing_window.hpp"
#include <catch.hpp>

#if !defined(RIVE_TOOLS_NO_GL) && (defined(__APPLE__) || defined(RIVE_ANDROID))
#define TEXEL_BUFFER_RING_GL_TEST_ACTIVE
#endif

#ifdef TEXEL_BUFFER_RING_GL_TEST_ACTIVE

#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/gpu.hpp"
#include "shaders/constants.glsl"

#ifdef __APPLE__
#include <dlfcn.h>
#endif

#include <algorithm>
#include <array>

namespace
{
constexpr int kW = 64;
constexpr int kH = 64;

// TestingWindow::Init aborts on a backend it cannot bring up, so probe first.
bool glBackendCanStart()
{
#ifdef __APPLE__
  // Held for the process; MakeEGL dlopens it again anyway.
  return dlopen("libEGL.dylib", RTLD_LAZY) != nullptr;
#else
  return true;
#endif
}

TestingWindow* initGLWindow()
{
#ifdef __APPLE__
  constexpr auto kBackend = TestingWindow::Backend::angle;
#else
  constexpr auto kBackend = TestingWindow::Backend::gl;
#endif
  return TestingWindow::Init(kBackend, {}, TestingWindow::Visibility::headless);
}

// Drives the RenderContext directly rather than through TestingWindow's frame
// path, which releases every GPU resource before each frame and would
// reallocate the rings under the test. Every path is its own rect, so
// pathCount paths need pathCount contours.
void drawFrame(
    rive::gpu::RenderContext* ctx,
    rive::gpu::RenderTarget* target,
    int pathCount)
{
  ctx->beginFrame({
      .renderTargetWidth = target->width(),
      .renderTargetHeight = target->height(),
      .clearColor = 0xff000000,
  });
  rive::RiveRenderer renderer(ctx);
  auto path = ctx->makeEmptyRenderPath();
  path->addRect(4, 4, 40, 40);
  auto paint = ctx->makeRenderPaint();
  paint->color(0xff00ff00);
  for (int i = 0; i < pathCount; ++i)
  {
    renderer.drawPath(path.get(), paint.get());
  }
  ctx->flush({.renderTarget = target});
  // Wait for the GPU so the next flushes find their slots' fences signaled
  // and reuse the ring's textures instead of allocating new ones.
  glFinish();
}

// bindToRenderContext() leaves the texture it uploaded into bound on the
// storage buffer's dedicated texture unit, and nothing rebinds that unit for
// the rest of the flush.
GLuint boundStorageTexture(uint32_t bindingIdx)
{
  GLint id = 0;
  glActiveTexture(GL_TEXTURE0 + bindingIdx);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &id);
  return static_cast<GLuint>(id);
}
} // namespace

TEST_CASE("TexelBufferRingWebGL rotates", "[rendering]")
{
  if (!glBackendCanStart())
  {
    WARN("no headless GLES available; skipping");
    return;
  }
  TestingWindow* window = initGLWindow();
  // A failed REQUIRE below must not leak the window into the next test.
  struct DestroyWindow
  {
    ~DestroyWindow() { TestingWindow::Destroy(); }
  } destroyWindow;
  if (window == nullptr || window->renderContextGLImpl() == nullptr)
  {
    WARN("GL backend did not start; skipping");
    return;
  }
  const auto& caps = window->renderContextGLImpl()->capabilities();
  if (caps.ARB_shader_storage_buffer_object)
  {
    WARN("driver has vertex-stage SSBOs; the texel polyfill is not in use "
         "here, skipping");
    return;
  }
  INFO(
      "GL_RENDERER: " << reinterpret_cast<const char*>(
          glGetString(GL_RENDERER)));

  window->resize(kW, kH);

  // One frame through the window creates its render target.
  window->beginFrame({.name = "texel_buffer_ring", .clearColor = 0});
  window->endFrame();
  rive::gpu::RenderContext* ctx = window->renderContext();
  rive::gpu::RenderTarget* target = window->renderTarget();
  REQUIRE(ctx != nullptr);
  REQUIRE(target != nullptr);

  // Warm up so the initial allocation is behind us. Identical frames from
  // here on keep the allocation steady, so texture IDs only change if the
  // ring rotates. Each frame here is one flush, so this sees the rotation
  // per flush; the advance within a multi-flush frame is checked below.
  // Large enough that the multi-flush frame below, with its per-flush
  // padding, fits the same allocation.
  constexpr int kSmallPathCount = 16;
  drawFrame(ctx, target, kSmallPathCount);
  drawFrame(ctx, target, kSmallPathCount);

  constexpr int kFrames = rive::gpu::kBufferRingSize * 2;
  std::array<GLuint, kFrames> pathTextures{}, contourTextures{};
  for (int i = 0; i < kFrames; ++i)
  {
    drawFrame(ctx, target, kSmallPathCount);
    pathTextures[i] = boundStorageTexture(PATH_BUFFER_IDX);
    contourTextures[i] = boundStorageTexture(CONTOUR_BUFFER_IDX);
  }

  const bool rotates = caps.avoidTexSubImage2DOnInFlightTextures;
  CAPTURE(rotates);
  for (const auto& textures : {pathTextures, contourTextures})
  {
    CAPTURE(textures);
    for (GLuint id : textures)
    {
      CHECK(id != 0);
    }
    // Consecutive flushes land in distinct textures when rotating, the
    // same one otherwise ...
    for (int i = 0; i < rive::gpu::kBufferRingSize; ++i)
    {
      for (int j = i + 1; j < rive::gpu::kBufferRingSize; ++j)
      {
        CHECK((textures[i] != textures[j]) == rotates);
      }
    }
    // ... and come back around after kBufferRingSize flushes.
    for (int i = rive::gpu::kBufferRingSize; i < kFrames; ++i)
    {
      CHECK(textures[i] == textures[i - rive::gpu::kBufferRingSize]);
    }
  }

  // A frame with several logical flushes binds once per logical flush off a
  // single map, and the slot advances per fenced flush, not per map. Every
  // logical flush binds the path ring for the path record it reserves, so two
  // logical flushes must move it on by two; per-map rotation would move it on
  // by one. The contour ring is only bound by flushes whose draws need
  // contours, so its count is not asserted.
  {
    const GLuint lastPath = pathTextures[kFrames - 1];
    ctx->beginFrame({
        .renderTargetWidth = target->width(),
        .renderTargetHeight = target->height(),
        .clearColor = 0xff000000,
    });
    rive::RiveRenderer renderer(ctx);
    auto path = ctx->makeEmptyRenderPath();
    path->addRect(4, 4, 40, 40);
    auto paint = ctx->makeRenderPaint();
    renderer.drawPath(path.get(), paint.get());
    ctx->logicalFlush();
    renderer.drawPath(path.get(), paint.get());
    ctx->flush({.renderTarget = target});

    auto advance = [&](const std::array<GLuint, kFrames>& textures,
                       GLuint from,
                       int binds) {
      const int i = static_cast<int>(
          std::find(textures.begin(), textures.end(), from) - textures.begin());
      return textures[(i + binds) % rive::gpu::kBufferRingSize];
    };
    CHECK(
        boundStorageTexture(PATH_BUFFER_IDX) ==
        advance(pathTextures, lastPath, 2));
  }
}

#endif // TEXEL_BUFFER_RING_GL_TEST_ACTIVE
