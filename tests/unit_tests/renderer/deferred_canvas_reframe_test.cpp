/*
 * Copyright 2026 Rive
 */

// A script drawn once per editor viewport opens one canvas twice a frame.

#include "deferred_test_sink.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/render_canvas.hpp"

#include <catch.hpp>

using namespace rive;
using namespace rive::cmd;

namespace
{
struct FakeTarget : gpu::RenderTarget
{
  FakeTarget() : RenderTarget(8, 8) {}
};

rcp<gpu::RenderCanvas> fakeCanvas()
{
  auto canvas = make_rcp<gpu::RenderCanvas>(8, 8);
  canvas->setBacking(nullptr, make_rcp<FakeTarget>());
  return canvas;
}

class CtmRenderer : public Renderer
{
public:
  std::vector<Mat2D> drawCtms;

  void save() override { m_stack.push_back(m_ctm); }
  void restore() override
  {
    if (!m_stack.empty())
    {
      m_ctm = m_stack.back();
      m_stack.pop_back();
    }
  }
  void transform(const Mat2D& m) override { m_ctm = m_ctm * m; }
  void drawPath(RenderPath*, RenderPaint*) override
  {
    drawCtms.push_back(m_ctm);
  }
  void clipPath(RenderPath*) override {}
  void drawImage(const RenderImage*, ImageSampler, BlendMode, float) override {}
  void drawImageMesh(
      const RenderImage*,
      ImageSampler,
      rcp<RenderBuffer>,
      rcp<RenderBuffer>,
      rcp<RenderBuffer>,
      uint32_t,
      uint32_t,
      BlendMode,
      float) override
  {}
  void modulateOpacity(float) override {}

private:
  Mat2D m_ctm;
  std::vector<Mat2D> m_stack;
};

class FrameSink : public deferred_test::TestSink
{
public:
  std::vector<std::unique_ptr<CtmRenderer>> frames;
  std::vector<gpu::RenderCanvas*> opened;
  int ended = 0;

  Renderer* beginCanvasContent(gpu::RenderCanvas* canvas, uint32_t) override
  {
    opened.push_back(canvas);
    frames.push_back(std::make_unique<CtmRenderer>());
    return frames.back().get();
  }
  void endCanvasContent() override { ended++; }
};

// A script frame that scales without save/restore.
void scriptFrame(
    DeferredSession& session,
    gpu::RenderCanvas* canvas,
    RenderPath* path,
    RenderPaint* paint)
{
  Renderer* r = session.beginCanvasContent(canvas, 0);
  Mat2D ctm;
  REQUIRE(r->currentTransform(&ctm));
  CHECK(ctm == Mat2D());
  r->transform(Mat2D::fromScale(3, 3));
  r->drawPath(path, paint);
  session.endCanvasContent(canvas);
}
} // namespace

TEST_CASE("a canvas's last frame replaces its earlier ones", "[cmd][canvas]")
{
  DeferredSession session(rive::ore::ReplayCaps{});
  auto canvas = fakeCanvas();
  auto path = session.makeEmptyRenderPath();
  auto paint = session.makeRenderPaint();

  scriptFrame(session, canvas.get(), path.get(), paint.get());
  scriptFrame(session, canvas.get(), path.get(), paint.get());
  session.closeOpenRange();

  auto frame = snapshotFrame(session);
  FrameSink sink;
  DeferredReplayer replayer;
  replayer.replayFrame(frame, sink);

  // One cleared frame at scale 3, not a merged 3 then 9.
  REQUIRE(sink.frames.size() == 1);
  CHECK(sink.frames[0]->drawCtms == std::vector<Mat2D>{Mat2D::fromScale(3, 3)});
  CHECK(sink.ended == 1);
}

TEST_CASE(
    "a reader between two frames of its source replays after it",
    "[cmd][canvas]")
{
  DeferredSession session(rive::ore::ReplayCaps{});
  auto source = fakeCanvas();
  auto reader = fakeCanvas();
  auto path = session.makeEmptyRenderPath();
  auto paint = session.makeRenderPaint();

  // Each viewport records the reader before the source it samples.
  for (int pass = 0; pass < 2; pass++)
  {
    Renderer* r = session.beginCanvasContent(reader.get(), 0);
    r->drawImage(source->renderImage(), {}, BlendMode::srcOver, 1.0f);
    session.endCanvasContent(reader.get());
    Renderer* s = session.beginCanvasContent(source.get(), 0);
    s->drawPath(path.get(), paint.get());
    session.endCanvasContent(source.get());
  }
  session.closeOpenRange();

  auto frame = snapshotFrame(session);
  FrameSink sink;
  DeferredReplayer replayer;
  replayer.replayFrame(frame, sink);

  CHECK(
      sink.opened ==
      std::vector<gpu::RenderCanvas*>{source.get(), reader.get()});
}

TEST_CASE(
    "a new frame never extends a range already on its canvas",
    "[cmd][canvas]")
{
  DeferredSession session(rive::ore::ReplayCaps{});
  auto canvas = fakeCanvas();
  auto path = session.makeEmptyRenderPath();
  auto paint = session.makeRenderPaint();

  Renderer* r = session.beginCanvasContent(canvas.get(), 0);
  session.endCanvasContent(canvas.get());
  r->drawPath(path.get(), paint.get()); // stale recorder reroutes here
  scriptFrame(session, canvas.get(), path.get(), paint.get());
  session.closeOpenRange();

  auto frame = snapshotFrame(session);
  FrameSink sink;
  DeferredReplayer replayer;
  replayer.replayFrame(frame, sink);

  REQUIRE(sink.frames.size() == 1);
  CHECK(sink.frames[0]->drawCtms == std::vector<Mat2D>{Mat2D::fromScale(3, 3)});
}

TEST_CASE(
    "a canvas range resumed after a nested canvas keeps its frame",
    "[cmd][canvas]")
{
  DeferredSession session(rive::ore::ReplayCaps{});
  auto outer = fakeCanvas();
  auto inner = fakeCanvas();
  auto path = session.makeEmptyRenderPath();
  auto paint = session.makeRenderPaint();

  // A nested bracket resumes the outer frame, which keeps its state.
  Renderer* o = session.beginCanvasContent(outer.get(), 0);
  o->transform(Mat2D::fromScale(2, 2));
  Renderer* i = session.beginCanvasContent(inner.get(), 0);
  i->drawPath(path.get(), paint.get());
  session.endCanvasContent(inner.get());
  o->drawPath(path.get(), paint.get());
  session.endCanvasContent(outer.get());
  session.closeOpenRange();

  auto frame = snapshotFrame(session);
  FrameSink sink;
  DeferredReplayer replayer;
  replayer.replayFrame(frame, sink);

  REQUIRE(sink.frames.size() == 2);
  size_t outerDraws = 0;
  for (auto& f : sink.frames)
  {
    for (const Mat2D& m : f->drawCtms)
    {
      if (m == Mat2D::fromScale(2, 2))
      {
        outerDraws++;
      }
    }
  }
  CHECK(outerDraws == 1);
  CHECK(sink.ended == 2);
}
