/*
 * Copyright 2026 Rive
 */

// Recording allocates no canvas backing, so replay backs each canvas on the
// sink's render context, and a sink without one drops the content.

#ifdef RIVE_CANVAS

#include "common/render_context_null.hpp"
#include "deferred_test_sink.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive_render_paint.hpp"

#include <catch.hpp>

using namespace rive;
using namespace rive::cmd;

namespace
{
class BackingSink : public deferred_test::TestSink
{
public:
  explicit BackingSink(std::unique_ptr<gpu::RenderContext> rc)
      : m_rc(std::move(rc))
  {}

  std::vector<gpu::RenderCanvas*> contentOpened;

  gpu::RenderContext* renderContext() override { return m_rc.get(); }

  Renderer* beginCanvasContent(gpu::RenderCanvas* canvas, uint32_t) override
  {
    contentOpened.push_back(canvas);
    return nullptr;
  }

private:
  std::unique_ptr<gpu::RenderContext> m_rc;
};

// Replays resources through the render context itself, so paints come back
// as the RiveRenderPaints the GPU draws with. Screen draws drop.
class RenderContextSink : public BackingSink
{
public:
  using BackingSink::BackingSink;

  Factory* factory() override { return renderContext(); }
  Renderer* beginScreenFrame(uint64_t) override { return nullptr; }
};

void recordCanvasDraw(DeferredSession& session, gpu::RenderCanvas* canvas)
{
  Renderer* content = session.beginCanvasContent(canvas, 0);
  auto paint = session.makeRenderPaint();
  auto path = session.makeEmptyRenderPath();
  content->drawPath(path.get(), paint.get());
  session.endCanvasContent(canvas);
  session.closeOpenRange();
}
} // namespace

TEST_CASE(
    "replay backs a recorded canvas on the sink's render context",
    "[deferred][canvas-backing]")
{
  DeferredSession session(rive::ore::ReplayCaps{});
  auto canvas = make_rcp<gpu::RenderCanvas>(8, 8);
  recordCanvasDraw(session, canvas.get());
  REQUIRE(!canvas->isBacked());

  BackingSink sink(RenderContextNULL::MakeContext());
  DeferredReplayer replayer;
  replayer.replayFrame(snapshotFrame(session), sink);

  CHECK(canvas->isBacked());
  CHECK(sink.contentOpened == std::vector<gpu::RenderCanvas*>{canvas.get()});
}

TEST_CASE(
    "a sink with no render context leaves the canvas unbacked",
    "[deferred][canvas-backing]")
{
  DeferredSession session(rive::ore::ReplayCaps{});
  auto canvas = make_rcp<gpu::RenderCanvas>(8, 8);
  recordCanvasDraw(session, canvas.get());

  BackingSink sink(nullptr);
  DeferredReplayer replayer;
  replayer.replayFrame(snapshotFrame(session), sink);

  CHECK(!canvas->isBacked());
  CHECK(sink.contentOpened.empty());
}

TEST_CASE(
    "a paint modulated by a canvas image gets the canvas's first texture",
    "[deferred][canvas-backing]")
{
  // A canvas sampled through a paint on the frame it is first drawn. Paint
  // mutations replay ahead of every canvas segment, so the paint must not
  // read the image before replay has backed it.
  DeferredSession session(rive::ore::ReplayCaps{});
  auto canvas = make_rcp<gpu::RenderCanvas>(8, 8);
  recordCanvasDraw(session, canvas.get());

  auto paint = session.makeRenderPaint();
  paint->modulatedImage(
      canvas->renderImage(),
      ImageSampler::LinearClamp(),
      Mat2D());
  auto path = session.makeEmptyRenderPath();
  session.screenRenderer()->drawPath(path.get(), paint.get());
  REQUIRE(!canvas->isBacked());

  RenderContextSink sink(RenderContextNULL::MakeContext());
  DeferredReplayer replayer;
  replayer.replayFrame(snapshotFrame(session), sink);
  REQUIRE(canvas->isBacked());

  auto* recorded = lite_rtti_cast<DeferredRenderPaint*>(paint.get());
  REQUIRE(recorded != nullptr);
  auto* replayed = lite_rtti_cast<RiveRenderPaint*>(
      replayer.table().paints.get(recorded->id()));
  REQUIRE(replayed != nullptr);
  CHECK(replayed->getImageTexture() != nullptr);
}

#endif
