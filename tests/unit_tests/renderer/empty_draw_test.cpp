/*
 * Copyright 2026 Rive
 */

// Paths that tessellate to nothing get dropped before they're sorted into
// drawGroups. An empty clip path clips out everything under it.

#include "common/render_context_null.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include <catch.hpp>
#include <map>

using namespace rive;
using namespace rive::gpu;

namespace
{
class CapturingRenderContext : public RenderContextNULL
{
public:
  void flush(const FlushDescriptor& desc) override
  {
    if (desc.drawList == nullptr)
    {
      return;
    }
    for (const DrawBatch& batch : *desc.drawList)
    {
      ++m_batchCounts[batch.drawType];
      // A dstBlend barrier that nothing reads would be a waste of a barrier.
      if (enums::is_flag_set(batch.barriers, BarrierFlags::dstBlend) &&
          batch.drawType != DrawType::renderPassInitialize)
      {
        CHECK(batch.dstReadList != nullptr);
      }
    }
  }

  size_t count(DrawType drawType) const
  {
    auto it = m_batchCounts.find(drawType);
    return it != m_batchCounts.end() ? it->second : 0;
  }

  size_t total() const
  {
    size_t n = 0;
    for (auto [drawType, count] : m_batchCounts)
    {
      if (drawType != DrawType::renderPassInitialize &&
          drawType != DrawType::renderPassResolve)
      {
        n += count;
      }
    }
    return n;
  }

private:
  std::map<DrawType, size_t> m_batchCounts;
};

constexpr uint32_t TargetSize = 256;

struct Frame
{
  CapturingRenderContext* capture;
  std::unique_ptr<RenderContext> ctx;
};

// Use depthStencil/inkbleed since it's a mode that issues dstRead barriers.
Frame beginDepthStencilFrame()
{
  auto impl = std::make_unique<CapturingRenderContext>();
  CapturingRenderContext* capture = impl.get();
  auto ctx = std::make_unique<RenderContext>(std::move(impl));
  ctx->beginFrame({
      .renderTargetWidth = TargetSize,
      .renderTargetHeight = TargetSize,
      .msaaSampleCount = 1,
      .clockwiseFillOverride = true,
      .inkbleedOverride = true,
  });
  REQUIRE(ctx->frameInterlockMode() == InterlockMode::depthStencil);
  return {capture, std::move(ctx)};
}

void flushFrame(Frame& frame)
{
  auto target = frame.capture->makeRenderTarget(TargetSize, TargetSize);
  frame.ctx->flush({.renderTarget = target.get()});
}

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

// Two contours that are each a single point: nonempty bounds (so the path
// survives bounds culling), but nothing to tessellate.
rcp<RenderPath> makeEmpty(RenderContext* ctx)
{
  auto path = ctx->makeEmptyRenderPath();
  path->moveTo(20, 20);
  path->close();
  path->moveTo(200, 200);
  path->close();
  return path;
}
} // namespace

TEST_CASE("empty paths are dropped (depthStencil)", "[RiveRenderer][empty]")
{
  Frame frame = beginDepthStencilFrame();
  auto paint = frame.ctx->makeRenderPaint();
  paint->color(0xff336699);
  RiveRenderer renderer(frame.ctx.get());
  renderer.drawPath(makeEmpty(frame.ctx.get()).get(), paint.get());
  flushFrame(frame);
  CHECK(frame.capture->total() == 0);
}

TEST_CASE(
    "an empty clip clips out everything (depthStencil)",
    "[RiveRenderer][empty]")
{
  Frame frame = beginDepthStencilFrame();
  auto paint = frame.ctx->makeRenderPaint();
  paint->color(0xff336699);
  RiveRenderer renderer(frame.ctx.get());
  renderer.save();
  renderer.clipPath(makeEmpty(frame.ctx.get()).get());
  renderer.drawPath(
      makeRect(frame.ctx.get(), 20, 20, 200, 200).get(),
      paint.get());
  renderer.restore();
  flushFrame(frame);
  CHECK(frame.capture->total() == 0);
}

TEST_CASE(
    "an empty advanced blend draw doesn't schedule a dstBlend barrier "
    "(depthStencil)",
    "[RiveRenderer][empty]")
{
  Frame frame = beginDepthStencilFrame();
  auto srcOver = frame.ctx->makeRenderPaint();
  srcOver->color(0xff336699);
  auto multiply = frame.ctx->makeRenderPaint();
  multiply->color(0xff996633);
  multiply->blendMode(BlendMode::multiply);
  RiveRenderer renderer(frame.ctx.get());
  // The empty advanced blend draw shares a drawGroup with real srcOver draws.
  renderer.drawPath(
      makeRect(frame.ctx.get(), 20, 20, 120, 120).get(),
      srcOver.get());
  renderer.drawPath(makeEmpty(frame.ctx.get()).get(), multiply.get());
  renderer.drawPath(
      makeRect(frame.ctx.get(), 140, 140, 240, 240).get(),
      srcOver.get());
  flushFrame(frame);
  CHECK(frame.capture->total() > 0);
}
