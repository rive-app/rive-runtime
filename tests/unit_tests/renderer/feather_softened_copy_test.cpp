/*
 * Copyright 2026 Rive
 */

// Tests that RiveRenderPath keeps the softened copy it makes for a feathered
// fill: an unchanged path drawn with a feather and scale that chop the same way
// gets the same copy back, and every change the copy depends on (geometry, fill
// rule, feather * scale) makes a new one while something still holds the old
// one, or rebuilds it in place once nothing does. Either way it matches a copy
// made from scratch.

#include "common/render_context_null.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive_render_path.hpp"
#include <catch.hpp>

namespace rive
{
namespace
{
void addCurvedContour(RiveRenderPath& path, float offset)
{
  path.moveTo(offset, 0);
  path.cubicTo(offset + 100, -50, offset + 150, 150, offset + 40, 120);
  path.lineTo(offset, 100);
  path.close();
}

// A softened copy made from scratch, by a path with the same geometry and fill
// rule that has never made one.
rcp<RiveRenderPath> freshSoftenedCopy(
    const RiveRenderPath& path,
    float feather,
    float matrixMaxScale)
{
  RiveRenderPath twin;
  twin.fillRule(path.getFillRule());
  twin.addRawPath(path.getRawPath());
  return twin.makeSoftenedCopyForFeathering(feather, matrixMaxScale);
}

bool matchesFresh(
    const rcp<RiveRenderPath>& copy,
    const RiveRenderPath& path,
    float feather,
    float matrixMaxScale)
{
  rcp<RiveRenderPath> fresh = freshSoftenedCopy(path, feather, matrixMaxScale);
  return copy->getRawPath() == fresh->getRawPath() &&
         copy->getFillRule() == fresh->getFillRule();
}

TEST_CASE("an unchanged path keeps its softened copy", "[feather]")
{
  RiveRenderPath path;
  addCurvedContour(path, 0);

  rcp<RiveRenderPath> first = path.makeSoftenedCopyForFeathering(8, 1);
  REQUIRE(first != nullptr);
  CHECK(matchesFresh(first, path, 8, 1));
  // The copy chops the curve, so it isn't the path itself.
  CHECK(first->getRawPath().verbs().size() > path.getRawPath().verbs().size());

  CHECK(path.makeSoftenedCopyForFeathering(8, 1).get() == first.get());
  CHECK(path.makeSoftenedCopyForFeathering(8, 1).get() == first.get());
}

TEST_CASE("every kind of mutation makes a new softened copy", "[feather]")
{
  RiveRenderPath path;
  addCurvedContour(path, 0);
  rcp<RiveRenderPath> last = path.makeSoftenedCopyForFeathering(8, 1);

  // 'last' is still held, as a draw would hold it, so every change makes a
  // new copy.
  auto expectNewAndCorrect = [&]() {
    rcp<RiveRenderPath> next = path.makeSoftenedCopyForFeathering(8, 1);
    CHECK(next.get() != last.get());
    CHECK(matchesFresh(next, path, 8, 1));
    // ...and the new one is what's kept from here on.
    CHECK(path.makeSoftenedCopyForFeathering(8, 1).get() == next.get());
    last = next;
  };

  // moveTo / lineTo / cubicTo / close.
  path.moveTo(200, 0);
  expectNewAndCorrect();
  path.lineTo(300, 0);
  expectNewAndCorrect();
  path.cubicTo(350, 50, 350, 150, 300, 200);
  expectNewAndCorrect();
  path.close();
  expectNewAndCorrect();

  // addRawPath, addRenderPath and addRenderPathBackwards.
  RiveRenderPath extra;
  addCurvedContour(extra, 400);
  path.addRawPath(extra.getRawPath());
  expectNewAndCorrect();
  path.addRenderPath(&extra, Mat2D::fromTranslate(0, 300));
  expectNewAndCorrect();
  path.addRenderPathBackwards(&extra, Mat2D::fromTranslate(0, 600));
  expectNewAndCorrect();

  // rewind, even back to the geometry it had.
  path.rewind();
  addCurvedContour(path, 0);
  expectNewAndCorrect();
}

TEST_CASE("a softened copy is kept per feather and scale", "[feather]")
{
  RiveRenderPath path;
  addCurvedContour(path, 0);

  rcp<RiveRenderPath> small = path.makeSoftenedCopyForFeathering(8, 1);
  rcp<RiveRenderPath> scaled = path.makeSoftenedCopyForFeathering(8, 4);
  CHECK(scaled.get() != small.get());
  CHECK(matchesFresh(scaled, path, 8, 4));

  // Only the last copy is kept, so going back makes another.
  rcp<RiveRenderPath> again = path.makeSoftenedCopyForFeathering(8, 1);
  CHECK(again.get() != small.get());
  CHECK(again.get() != scaled.get());
  CHECK(matchesFresh(again, path, 8, 1));

  // The copy only depends on feather * scale, so a feather that's half as
  // wide drawn twice as large gets the same one, and it's still correct.
  rcp<RiveRenderPath> wide = path.makeSoftenedCopyForFeathering(32, 1);
  CHECK(path.makeSoftenedCopyForFeathering(16, 2).get() == wide.get());
  CHECK(matchesFresh(wide, path, 16, 2));
}

TEST_CASE("a softened copy follows the path's fill rule", "[feather]")
{
  RiveRenderPath path;
  addCurvedContour(path, 0);

  rcp<RiveRenderPath> nonZero = path.makeSoftenedCopyForFeathering(8, 1);
  CHECK(nonZero->getFillRule() == FillRule::nonZero);

  path.fillRule(FillRule::evenOdd);
  rcp<RiveRenderPath> evenOdd = path.makeSoftenedCopyForFeathering(8, 1);
  CHECK(evenOdd.get() != nonZero.get());
  CHECK(evenOdd->getFillRule() == FillRule::evenOdd);
  CHECK(matchesFresh(evenOdd, path, 8, 1));
}

TEST_CASE("a softened copy nothing else holds is rebuilt in place", "[feather]")
{
  RiveRenderPath path;
  addCurvedContour(path, 0);
  // Only the path holds its copy once this temporary is gone.
  const RiveRenderPath* kept = path.makeSoftenedCopyForFeathering(8, 1).get();
  const uint64_t keptMutationID = kept->getRawPathMutationID();
  const size_t verbCapacity = kept->getRawPath().verbCapacity();
  const size_t pointCapacity = kept->getRawPath().pointCapacity();

  // The same shape moved: as many verbs and points as before, so the copy's
  // storage is reused as is.
  path.rewind();
  addCurvedContour(path, 10);
  {
    rcp<RiveRenderPath> moved = path.makeSoftenedCopyForFeathering(8, 1);
    CHECK(moved.get() == kept);
    CHECK(moved->getRawPathMutationID() != keptMutationID);
    CHECK(moved->getRawPath().verbCapacity() == verbCapacity);
    CHECK(moved->getRawPath().pointCapacity() == pointCapacity);
    CHECK(matchesFresh(moved, path, 8, 1));
    CHECK(moved->getBounds() == freshSoftenedCopy(path, 8, 1)->getBounds());
  }

  // A new scale and a new fill rule are rebuilt in place too.
  {
    rcp<RiveRenderPath> scaled = path.makeSoftenedCopyForFeathering(8, 4);
    CHECK(scaled.get() == kept);
    CHECK(matchesFresh(scaled, path, 8, 4));
  }
  path.fillRule(FillRule::evenOdd);
  {
    rcp<RiveRenderPath> evenOdd = path.makeSoftenedCopyForFeathering(8, 4);
    CHECK(evenOdd.get() == kept);
    CHECK(evenOdd->getFillRule() == FillRule::evenOdd);
    CHECK(matchesFresh(evenOdd, path, 8, 4));
  }
}
} // namespace

namespace gpu
{
namespace
{
struct FeatherFrames
{
  std::unique_ptr<RenderContext> context = RenderContextNULL::MakeContext();
  rcp<RenderTarget> target = context->static_impl_cast<RenderContextNULL>()
                                 ->makeRenderTarget(400, 400);

  // The renderer skips feathered fills that aren't clockwise, so these
  // nonZero paths only draw with the clockwise override on.
  void begin()
  {
    context->beginFrame({
        .renderTargetWidth = 400,
        .renderTargetHeight = 400,
        .clockwiseFillOverride = true,
    });
  }
  void flush() { context->flush({.renderTarget = target.get()}); }
};

TEST_CASE("a feathered fill builds its softened copy once", "[feather]")
{
  FeatherFrames frames;
  rcp<RenderPath> renderPath = frames.context->makeEmptyRenderPath();
  auto* path = static_cast<RiveRenderPath*>(renderPath.get());
  addCurvedContour(*path, 50);
  rcp<RenderPaint> paint = frames.context->makeRenderPaint();
  paint->color(0xffffffff);
  paint->feather(32);

  // Drawn with an identity matrix, so the renderer softens it with a scale
  // of 1, and asking for that copy here returns the one it drew. The path,
  // the draw and `drawn` hold it until the flush lets the draw go.
  frames.begin();
  {
    RiveRenderer renderer(frames.context.get());
    renderer.drawPath(path, paint.get());
  }
  rcp<RiveRenderPath> drawn = path->makeSoftenedCopyForFeathering(32, 1);
  CHECK(drawn->debugging_refcnt() == 3);
  frames.flush();
  CHECK(drawn->debugging_refcnt() == 2);

  // The next frame's draw holds the same copy.
  frames.begin();
  {
    RiveRenderer renderer(frames.context.get());
    renderer.drawPath(path, paint.get());
  }
  CHECK(drawn->debugging_refcnt() == 3);
  frames.flush();
  CHECK(path->makeSoftenedCopyForFeathering(32, 1).get() == drawn.get());
}

TEST_CASE(
    "a feathered fill that changes every frame keeps one softened copy",
    "[feather]")
{
  FeatherFrames frames;
  rcp<RenderPath> renderPath = frames.context->makeEmptyRenderPath();
  auto* path = static_cast<RiveRenderPath*>(renderPath.get());
  rcp<RenderPaint> paint = frames.context->makeRenderPaint();
  paint->color(0xffffffff);
  paint->feather(32);

  // Each flush lets go of the frame's draws, so the next frame's change
  // rebuilds the same copy.
  const RiveRenderPath* first = nullptr;
  for (int frame = 0; frame < 4; ++frame)
  {
    path->rewind();
    addCurvedContour(*path, 50 + 10 * static_cast<float>(frame));
    frames.begin();
    {
      RiveRenderer renderer(frames.context.get());
      renderer.drawPath(path, paint.get());
    }
    rcp<RiveRenderPath> drawn = path->makeSoftenedCopyForFeathering(32, 1);
    CHECK(drawn->debugging_refcnt() == 3);
    frames.flush();
    CHECK(drawn->debugging_refcnt() == 2);
    CHECK(matchesFresh(drawn, *path, 32, 1));
    if (frame == 0)
    {
      first = drawn.get();
    }
    CHECK(drawn.get() == first);
  }
}

TEST_CASE("a path can change while a draw holds its softened copy", "[feather]")
{
  FeatherFrames frames;
  rcp<RenderPath> renderPath = frames.context->makeEmptyRenderPath();
  auto* path = static_cast<RiveRenderPath*>(renderPath.get());
  addCurvedContour(*path, 50);
  rcp<RenderPaint> paint = frames.context->makeRenderPaint();
  paint->color(0xffffffff);
  paint->feather(32);

  // The first draw holds the first copy until the flush. Changing the path
  // makes the second draw soften it again, into a new copy, and leaves the
  // copy the first draw holds as it was. Only the path and the draws hold
  // copies here, so the first draw's is the one keeping it from being rebuilt
  // in place.
  frames.begin();
  {
    RiveRenderer renderer(frames.context.get());
    renderer.drawPath(path, paint.get());
    const RiveRenderPath* held = path->makeSoftenedCopyForFeathering(32, 1)
                                     .get();
    CHECK(held->debugging_refcnt() == 2);
    const RawPath heldGeometry = held->getRawPath();

    addCurvedContour(*path, 200);
    renderer.drawPath(path, paint.get());
    rcp<RiveRenderPath> second = path->makeSoftenedCopyForFeathering(32, 1);
    CHECK(second.get() != held);
    CHECK(held->debugging_refcnt() == 1);
    CHECK(held->getRawPath() == heldGeometry);
    CHECK(matchesFresh(second, *path, 32, 1));
  }
  frames.flush();
}
} // namespace
} // namespace gpu
} // namespace rive
