/*
 * Copyright 2026 Rive
 */

// A host that replays on a worker records and tears down on its own thread
// while the worker draws. Everything GPU backed has to reach the worker, which
// is the only thread allowed to free it.

#ifdef RIVE_CANVAS

#include "common/render_context_null.hpp"
#include "deferred_test_sink.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/render_context.hpp"

#include <catch.hpp>
#include <memory>
#include <optional>
#include <thread>

using namespace rive;
using namespace rive::cmd;

namespace
{
// Backs canvases on a context the worker owns.
class WorkerSink : public deferred_test::TestSink
{
public:
  explicit WorkerSink(gpu::RenderContext* rc) : m_rc(rc) {}
  gpu::RenderContext* renderContext() override { return m_rc; }
  Renderer* beginCanvasContent(gpu::RenderCanvas*, uint32_t) override
  {
    return nullptr;
  }

private:
  gpu::RenderContext* m_rc;
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

void replayOnWorker(
    DeferredReplayer& replayer,
    std::shared_ptr<DeferredFrame> frame,
    DeferredFrameSink& sink)
{
  std::thread worker([&replayer, &sink, frame = std::move(frame)]() mutable {
    replayer.replayFrame(*frame, sink);
    frame.reset();
  });
  worker.join();
}

// Takes what the dying session still holds, as a worker host has to before
// the session closes its retirer.
class LeftoverHost : public DeferredSessionAttachment
{
public:
  void join(DeferredSession* s)
  {
    session = s;
    s->attach(this);
  }
  void deferredSessionDestroyed() override
  {
    leftover.emplace(takeFrame(*session));
    session = nullptr;
  }

  DeferredSession* session = nullptr;
  std::optional<DeferredFrame> leftover;
};
} // namespace

TEST_CASE(
    "a taken frame replays on another thread after its session is gone",
    "[deferred][worker]")
{
  auto rc = RenderContextNULL::MakeContext();
  auto canvas = make_rcp<gpu::RenderCanvas>(8, 8);
  std::shared_ptr<DeferredFrame> frame;
  {
    DeferredSession session(ore::ReplayCaps{});
    recordCanvasDraw(session, canvas.get());
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    session.screenRenderer()->drawPath(path.get(), paint.get());
    frame = std::make_shared<DeferredFrame>(takeFrame(session));
  }

  WorkerSink sink(rc.get());
  DeferredReplayer replayer;
  replayOnWorker(replayer, std::move(frame), sink);
  CHECK(canvas->isBacked());
  CHECK(sink.openedTargets() == 1);
}

TEST_CASE(
    "a canvas dropped while recording frees its backing with the next "
    "frame",
    "[deferred][worker]")
{
  auto rc = RenderContextNULL::MakeContext();
  DeferredSession session(ore::ReplayCaps{});
  session.bindRenderContext(rc.get());
  WorkerSink sink(rc.get());
  DeferredReplayer replayer;

  auto canvas = session.makeContentCanvas(8, 8);
  recordCanvasDraw(session, canvas.get());
  replayOnWorker(
      replayer,
      std::make_shared<DeferredFrame>(takeFrame(session)),
      sink);
  REQUIRE(canvas->isBacked());
  rcp<gpu::RenderTarget> backing = ref_rcp(canvas->renderTarget());

  canvas = nullptr;
  CHECK(backing->debugging_refcnt() > 1);
  auto next = std::make_shared<DeferredFrame>(takeFrame(session));
  CHECK(!next->retiredCanvasBackings.empty());
  replayOnWorker(replayer, std::move(next), sink);
  CHECK(backing->debugging_refcnt() == 1);
}

TEST_CASE(
    "a dying session hands its unreplayed frame to the host",
    "[deferred][worker]")
{
  auto rc = RenderContextNULL::MakeContext();
  WorkerSink sink(rc.get());
  DeferredReplayer replayer;
  LeftoverHost host;
  rcp<gpu::RenderTarget> retired;
  {
    DeferredSession session(ore::ReplayCaps{});
    session.bindRenderContext(rc.get());
    host.join(&session);

    auto dropped = session.makeContentCanvas(8, 8);
    recordCanvasDraw(session, dropped.get());
    replayOnWorker(
        replayer,
        std::make_shared<DeferredFrame>(takeFrame(session)),
        sink);
    REQUIRE(dropped->isBacked());
    retired = ref_rcp(dropped->renderTarget());
    dropped = nullptr;

    // Recorded, never replayed.
    auto pending = session.makeContentCanvas(8, 8);
    recordCanvasDraw(session, pending.get());
  }

  REQUIRE(host.leftover.has_value());
  CHECK(host.session == nullptr);
  CHECK(host.leftover->contentCanvases.size() == 1);
  CHECK(retired->debugging_refcnt() > 1);
  replayOnWorker(
      replayer,
      std::make_shared<DeferredFrame>(std::move(*host.leftover)),
      sink);
  CHECK(retired->debugging_refcnt() == 1);
}

#endif
