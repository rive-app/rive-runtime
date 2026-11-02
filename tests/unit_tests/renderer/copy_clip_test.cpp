/*
 * Copyright 2026 Rive
 */

// RiveRenderer::copyClipTo: a fresh renderer issuing a draw on behalf of one
// whose frame was interrupted (WebGL's offscreen composites) has to be clipped
// as the original would have been.

#include "common/render_context_null.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include <catch.hpp>

using namespace rive;
using namespace rive::gpu;

namespace
{
RenderContext::FrameDescriptor s_frameDescriptor = {
    .renderTargetWidth = 100,
    .renderTargetHeight = 100,
};

void flushFrame(RenderContext* ctx)
{
  auto renderTarget = ctx->static_impl_cast<RenderContextNULL>()
                          ->makeRenderTarget(
                              s_frameDescriptor.renderTargetWidth,
                              s_frameDescriptor.renderTargetHeight);
  ctx->flush({.renderTarget = renderTarget.get()});
}

rcp<RenderPath> makeOval(RenderContext* ctx, const AABB& bounds)
{
  RawPath path;
  path.addOval(bounds);
  return ctx->makeRenderPath(path, FillRule::nonZero);
}

rcp<RenderPath> makeRect(RenderContext* ctx, const AABB& bounds)
{
  RawPath path;
  path.addRect(bounds);
  return ctx->makeRenderPath(path, FillRule::nonZero);
}
} // namespace

TEST_CASE("a copied clip path clips the copy's draws", "[RiveRenderer][clip]")
{
  auto ctx = RenderContextNULL::MakeContext();
  ctx->beginFrame(s_frameDescriptor);
  auto clip = makeOval(ctx.get(), {10, 10, 60, 60});
  auto shape = makeRect(ctx.get(), {0, 0, 100, 100});
  auto paint = ctx->makeRenderPaint();
  paint->color(0xffffffff);

  RiveRenderer original(ctx.get());
  original.save();
  original.translate(5, 5);
  original.clipPath(clip.get());
  // Gives the element a clip ID in this flush.
  original.drawPath(shape.get(), paint.get());
  REQUIRE(ctx->getClipContentID() != 0);

  // What an offscreen raster does to the frame in the middle of a draw.
  flushFrame(ctx.get());
  ctx->beginFrame(s_frameDescriptor);
  REQUIRE(ctx->getClipContentID() == 0);

  RiveRenderer unclipped(ctx.get());
  unclipped.drawPath(shape.get(), paint.get());
  CHECK(ctx->getClipContentID() == 0);

  RiveRenderer fresh(ctx.get());
  fresh.save();
  original.copyClipTo(&fresh);
  Mat2D ctm;
  REQUIRE(fresh.currentTransform(&ctm));
  CHECK(ctm == Mat2D());
  fresh.drawPath(shape.get(), paint.get());
  // The copy drew its own clip element rather than trusting the stale ID.
  CHECK(ctx->getClipContentID() != 0);
  fresh.restore();
  original.restore();
  flushFrame(ctx.get());
}

TEST_CASE(
    "a copied clip rect keeps its rect and matrix",
    "[RiveRenderer][clip]")
{
  auto ctx = RenderContextNULL::MakeContext();
  ctx->beginFrame(s_frameDescriptor);
  REQUIRE(ctx->frameSupportsClipRects());
  auto rect = makeRect(ctx.get(), {10, 20, 30, 40});

  RiveRenderer original(ctx.get());
  original.scale(2, 2);
  original.clipPath(rect.get());
  REQUIRE(original.hasClipRect());

  flushFrame(ctx.get());
  ctx->beginFrame(s_frameDescriptor);

  RiveRenderer fresh(ctx.get());
  original.copyClipTo(&fresh);
  REQUIRE(fresh.hasClipRect());
  CHECK(fresh.getClipRect() == original.getClipRect());
  CHECK(fresh.getClipRectMatrix() == Mat2D::fromScale(2, 2));
  flushFrame(ctx.get());
}

// The interrupted renderer itself keeps drawing after the composite, so its
// own per-flush clip state has to go stale with the flush rather than alias
// whatever the resumed frame puts in the same place.
TEST_CASE(
    "a clip ID from an earlier flush is not trusted",
    "[RiveRenderer][clip]")
{
  auto ctx = RenderContextNULL::MakeContext();
  ctx->beginFrame(s_frameDescriptor);
  auto clipA = makeOval(ctx.get(), {10, 10, 60, 60});
  auto clipB = makeOval(ctx.get(), {40, 40, 90, 90});
  auto shape = makeRect(ctx.get(), {0, 0, 100, 100});
  auto paint = ctx->makeRenderPaint();
  paint->color(0xffffffff);

  RiveRenderer interrupted(ctx.get());
  interrupted.clipPath(clipA.get());
  interrupted.drawPath(shape.get(), paint.get());
  const uint32_t staleID = ctx->getClipContentID();
  REQUIRE(staleID != 0);

  flushFrame(ctx.get());
  ctx->beginFrame(s_frameDescriptor);

  // Another clip takes the same ID in the new flush.
  RiveRenderer other(ctx.get());
  other.clipPath(clipB.get());
  other.drawPath(shape.get(), paint.get());
  REQUIRE(ctx->getClipContentID() == staleID);

  // Matching it would draw under clipB; clipA has to be drawn again.
  interrupted.drawPath(shape.get(), paint.get());
  CHECK(ctx->getClipContentID() != staleID);
  flushFrame(ctx.get());
}

TEST_CASE("a clip rect from an earlier frame is remade", "[RiveRenderer][clip]")
{
  auto ctx = RenderContextNULL::MakeContext();
  ctx->beginFrame(s_frameDescriptor);
  REQUIRE(ctx->frameSupportsClipRects());
  auto rectA = makeRect(ctx.get(), {10, 20, 30, 40});
  auto rectB = makeRect(ctx.get(), {50, 50, 90, 70});
  auto shape = makeRect(ctx.get(), {0, 0, 100, 100});
  auto paint = ctx->makeRenderPaint();
  paint->color(0xffffffff);

  RiveRenderer interrupted(ctx.get());
  interrupted.clipPath(rectA.get());
  REQUIRE(interrupted.hasClipRect());
  const Mat2D expected = gpu::ClipRectInverseMatrix(Mat2D(), {10, 20, 30, 40})
                             .inverseMatrix();

  flushFrame(ctx.get());
  ctx->beginFrame(s_frameDescriptor);

  // Allocates from the recycled per-frame memory.
  RiveRenderer other(ctx.get());
  other.clipPath(rectB.get());
  other.drawPath(shape.get(), paint.get());

  interrupted.drawPath(shape.get(), paint.get());
  REQUIRE(interrupted.getClipRectInverseMatrix() != nullptr);
  CHECK(interrupted.getClipRectInverseMatrix()->inverseMatrix() == expected);
  flushFrame(ctx.get());
}
