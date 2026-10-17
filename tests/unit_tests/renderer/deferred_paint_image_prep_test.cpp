/*
 * Copyright 2026 Rive
 */

// A backend whose images upload lazily (web) readies the image a paint samples
// through DeferredFrameSink::prepForeignImage, which comes back null until the
// decode lands. The producer records an unchanged paint image only once, so
// replay has to retry it on later frames.

#include "assets/nomoon.png.hpp"
#include "common/render_context_null.hpp"
#include "deferred_test_sink.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive_render_paint.hpp"
#include "utils/no_op_renderer.hpp"

#include <catch.hpp>
#include <vector>

using namespace rive;
using namespace rive::cmd;

namespace
{
// Records, per path drawn, whether the paint carries its image.
class ImageSpyRenderer : public NoOpRenderer
{
public:
  std::vector<bool> drawsWithImage;

  void drawPath(RenderPath*, RenderPaint* paint) override
  {
    auto* rivePaint = lite_rtti_cast<RiveRenderPaint*>(paint);
    drawsWithImage.push_back(
        rivePaint != nullptr && rivePaint->getImageTexture() != nullptr);
  }
};

// Replays through a null render context, so paints and images come back as
// the renderer's own types, and holds every image back until `decoded`, like
// a browser decode still in flight.
class LazyImageSink : public deferred_test::TestSink
{
public:
  bool decoded = false;
  ImageSpyRenderer screen;

  LazyImageSink() : m_rc(RenderContextNULL::MakeContext()) {}

  gpu::RenderContext* renderContext() override { return m_rc.get(); }
  Factory* factory() override { return m_rc.get(); }
  Renderer* beginScreenFrame(uint64_t) override { return &screen; }
  RenderImage* prepForeignImage(RenderImage* image) override
  {
    return decoded ? image : nullptr;
  }

private:
  std::unique_ptr<gpu::RenderContext> m_rc;
};

struct PaintImageScene
{
  DeferredSession session{rive::ore::ReplayCaps{}};
  LazyImageSink sink;
  DeferredReplayer replayer;
  rcp<RenderImage> image = session.decodeImage(assets::nomoon_png());
  rcp<RenderPaint> paint = session.makeRenderPaint();
  rcp<RenderPath> path = session.makeEmptyRenderPath();

  PaintImageScene()
  {
    REQUIRE(image != nullptr);
    path->moveTo(0, 0);
    path->lineTo(10, 0);
    path->lineTo(10, 10);
    path->close();
  }

  // A shape paint re-applies its image every draw; the producer absorbs
  // the unchanged re-sets.
  void draw()
  {
    paint->modulatedImage(image.get(), ImageSampler::LinearClamp(), Mat2D());
    session.screenRenderer()->drawPath(path.get(), paint.get());
  }

  std::vector<bool> replay()
  {
    sink.screen.drawsWithImage.clear();
    replayer.replayFrame(takeFrame(session), sink);
    return sink.screen.drawsWithImage;
  }
};
} // namespace

TEST_CASE(
    "a paint takes its image on the first frame the image is ready",
    "[deferred][paint-image]")
{
  PaintImageScene scene;

  scene.draw();
  CHECK(scene.replay() == std::vector<bool>{false});
  scene.draw();
  CHECK(scene.replay() == std::vector<bool>{false});

  scene.sink.decoded = true;
  scene.draw();
  CHECK(scene.replay() == std::vector<bool>{true});
}

TEST_CASE(
    "a draw ahead of a paint version bump gets the image that landed "
    "between frames",
    "[deferred][paint-image]")
{
  PaintImageScene scene;

  scene.draw();
  CHECK(scene.replay() == std::vector<bool>{false});

  // The decode lands between frames, and the next frame draws the paint,
  // changes it (the first draw pinned the old version, so this mints a new
  // one), and draws it again. Both draws must carry the image.
  scene.sink.decoded = true;
  scene.draw();
  scene.paint->color(0xff00ff00);
  scene.draw();
  CHECK(scene.replay() == std::vector<bool>{true, true});
}
