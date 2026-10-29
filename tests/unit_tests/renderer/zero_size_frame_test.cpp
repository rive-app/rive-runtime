/*
 * Copyright 2026 Rive
 */

// A zero-area frame (e.g. a collapsed canvas) never reaches the backend.

#include "common/render_context_null.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include <catch.hpp>

using namespace rive;
using namespace rive::gpu;

namespace
{
class CountingRenderContext : public RenderContextNULL
{
public:
  void flush(const FlushDescriptor& desc) override
  {
    ++flushCount;
    if (desc.drawList == nullptr)
    {
      return;
    }
    for (const DrawBatch& batch : *desc.drawList)
    {
      if (batch.drawType != DrawType::renderPassInitialize &&
          batch.drawType != DrawType::renderPassResolve)
      {
        ++drawCount;
      }
    }
  }

  size_t flushCount = 0;
  size_t drawCount = 0;
};

rcp<RenderPath> makeRect(RenderContext* ctx, float l, float t, float r, float b)
{
  auto path = ctx->makeEmptyRenderPath();
  path->moveTo(l, t);
  path->lineTo(r, t);
  path->lineTo(r, b);
  path->lineTo(l, b);
  path->close();
  return path;
}

struct Counts
{
  size_t flushes;
  size_t draws;
};

Counts drawOverOrigin(
    uint32_t width,
    uint32_t height,
    RenderContext::FrameDescriptor desc)
{
  auto impl = std::make_unique<CountingRenderContext>();
  CountingRenderContext* counter = impl.get();
  auto ctx = std::make_unique<RenderContext>(std::move(impl));
  desc.renderTargetWidth = width;
  desc.renderTargetHeight = height;
  ctx->beginFrame(desc);
  CHECK(
      ctx->isOutsideCurrentFrame({-5, -5, 10, 10}) ==
      (width == 0 || height == 0));

  auto paint = ctx->makeRenderPaint();
  paint->color(0xff336699);
  RiveRenderer renderer(ctx.get());
  renderer.drawPath(makeRect(ctx.get(), -10, -10, 10, 10).get(), paint.get());
  renderer.save();
  renderer.clipPath(makeRect(ctx.get(), -8, -8, 8, 8).get());
  renderer.drawPath(makeRect(ctx.get(), -10, -10, 10, 10).get(), paint.get());
  renderer.restore();

  auto target = counter->makeRenderTarget(width, height);
  ctx->flush({.renderTarget = target.get()});
  return {counter->flushCount, counter->drawCount};
}
} // namespace

TEST_CASE("a zero-area frame draws nothing", "[RiveRenderer][empty]")
{
  const RenderContext::FrameDescriptor descs[] = {
      {},
      {.disableRasterOrdering = true},
      {.msaaSampleCount = 4},
      {.clockwiseFillOverride = true},
      {.disableRasterOrdering = true, .clockwiseFillOverride = true},
  };
  for (const auto& desc : descs)
  {
    CHECK(drawOverOrigin(0, 0, desc).flushes == 0);
    CHECK(drawOverOrigin(1, 0, desc).flushes == 0);
    CHECK(drawOverOrigin(0, 1, desc).flushes == 0);
    CHECK(drawOverOrigin(1, 1, desc).draws > 0);
  }
}
