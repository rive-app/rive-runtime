/*
 * Copyright 2026 Rive
 */

// Sampler canvases replay after the canvases they sample regardless of
// record order. Pure byte math, no GPU.

#include "deferred_test_sink.hpp"
#include "rive/renderer/cmd/canvas_schedule.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/render_canvas.hpp"

#include <catch.hpp>

using namespace rive;
using namespace rive::cmd;
using Target = DeferredSegment::Target;

namespace
{
// A hand-built 2D stream plus its canvas segments.
struct StreamBuilder
{
  std::vector<uint8_t> bytes;
  std::vector<DeferredSegment> segments;

  template <typename POD> void append(RenderCmd c, const POD& pod)
  {
    bytes.push_back(static_cast<uint8_t>(c));
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&pod);
    bytes.insert(bytes.end(), p, p + sizeof(POD));
  }

  // Records a canvas bracket holding the given flagged image samples.
  void canvasRange(
      uint64_t canvasId,
      std::initializer_list<uint64_t> sampledCanvasIds)
  {
    uint32_t begin = static_cast<uint32_t>(bytes.size());
    // Noise the walker must skip.
    DrawPathPOD path = {};
    append(RenderCmd::drawPath, path);
    for (uint64_t sampled : sampledCanvasIds)
    {
      DrawImagePOD draw = {};
      draw.image = kCanvasHandleFlag | static_cast<RenderHandle>(sampled);
      append(RenderCmd::drawImage, draw);
    }
    segments.push_back(
        {Target::canvas, canvasId, begin, static_cast<uint32_t>(bytes.size())});
  }

  // Records a canvas bracket filling a path whose paint samples a canvas,
  // the way the recorder names it after the draw.
  void canvasRangeDrawingPaintSampling(uint64_t canvasId, uint64_t sampled)
  {
    uint32_t begin = static_cast<uint32_t>(bytes.size());
    DrawPathPOD draw = {};
    append(RenderCmd::drawPath, draw);
    append(
        RenderCmd::canvasSample,
        ResIdPOD{kCanvasHandleFlag | static_cast<RenderHandle>(sampled)});
    segments.push_back(
        {Target::canvas, canvasId, begin, static_cast<uint32_t>(bytes.size())});
  }

  // A foreign image draw that is not a written canvas (host image).
  void canvasRangeSamplingForeign(uint64_t canvasId, uint32_t foreignIndex)
  {
    uint32_t begin = static_cast<uint32_t>(bytes.size());
    DrawImagePOD draw = {};
    draw.image = kCanvasHandleFlag | foreignIndex;
    append(RenderCmd::drawImage, draw);
    segments.push_back(
        {Target::canvas, canvasId, begin, static_cast<uint32_t>(bytes.size())});
  }

  std::vector<DeferredOreSegment> ore;

  // An Ore run recorded at this point in the 2D stream.
  void oreRun(
      std::initializer_list<uint64_t> reads,
      std::initializer_list<uint64_t> writes,
      bool pinned = false)
  {
    uint32_t begin = ore.empty() ? 0 : ore.back().end;
    ore.push_back(
        {begin,
         begin + 16,
         static_cast<uint32_t>(bytes.size()),
         reads,
         writes,
         pinned});
  }

  CanvasSchedule schedule() const
  {
    return scheduleCanvases(
        Span<const uint8_t>(bytes.data(), bytes.size()),
        segments,
        ore);
  }
};

bool sameRuns(
    const std::vector<CanvasSchedule::OreRun>& runs,
    std::initializer_list<CanvasSchedule::OreRun> expected)
{
  if (runs.size() != expected.size())
  {
    return false;
  }
  size_t i = 0;
  for (const CanvasSchedule::OreRun& want : expected)
  {
    const CanvasSchedule::OreRun& got = runs[i++];
    if (got.before != want.before || got.begin != want.begin ||
        got.end != want.end)
    {
      return false;
    }
  }
  return true;
}
} // namespace

TEST_CASE("in-order sampler keeps record order", "[cmd][canvas-dag]")
{
  StreamBuilder b;
  b.canvasRange(1, {});  // A writes
  b.canvasRange(2, {1}); // B samples A, recorded after
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{1, 2});
  CHECK_FALSE(s.hadCycle);
}

TEST_CASE("reader recorded before its writer reorders", "[cmd][canvas-dag]")
{
  StreamBuilder b;
  b.canvasRange(2, {1}); // B samples A but records first
  b.canvasRange(1, {});  // A writes
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{1, 2});
  CHECK_FALSE(s.hadCycle);
}

TEST_CASE(
    "reversed three-canvas chain schedules writer first",
    "[cmd][canvas-dag]")
{
  StreamBuilder b;
  b.canvasRange(3, {2}); // C samples B
  b.canvasRange(2, {1}); // B samples A
  b.canvasRange(1, {});  // A writes last in record order
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{1, 2, 3});
}

TEST_CASE("cycle demotes to record order and flags", "[cmd][canvas-dag]")
{
  StreamBuilder b;
  b.canvasRange(1, {2}); // A samples B
  b.canvasRange(2, {1}); // B samples A
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{1, 2});
  CHECK(s.hadCycle);
}

TEST_CASE("self sample is a demoted edge, not a reorder", "[cmd][canvas-dag]")
{
  StreamBuilder b;
  b.canvasRange(1, {1});
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{1});
  CHECK(s.hadCycle);
}

TEST_CASE("sampling an unwritten id adds no edge", "[cmd][canvas-dag]")
{
  StreamBuilder b;
  b.canvasRangeSamplingForeign(1, 7); // host image or unwritten canvas
  b.canvasRange(2, {});
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{1, 2});
  CHECK_FALSE(s.hadCycle);
}

TEST_CASE(
    "a read between two writes of one canvas sees its finished frame",
    "[cmd][canvas-dag]")
{
  StreamBuilder b;
  b.canvasRange(2, {});  // B opens first
  b.canvasRange(1, {});  // A
  b.canvasRange(2, {1}); // B samples A mid-frame
  b.canvasRange(1, {});  // A writes again
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{1, 2});
  CHECK_FALSE(s.hadCycle);
}

TEST_CASE("drawImageMesh creates edges like drawImage", "[cmd][canvas-dag]")
{
  StreamBuilder b;
  uint32_t begin = static_cast<uint32_t>(b.bytes.size());
  DrawImageMeshPOD mesh = {};
  mesh.image = kCanvasHandleFlag | 1u;
  b.append(RenderCmd::drawImageMesh, mesh);
  b.segments.push_back(
      {Target::canvas, 2, begin, static_cast<uint32_t>(b.bytes.size())});
  b.canvasRange(1, {});
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{1, 2});
}

TEST_CASE("canvasSample creates edges like drawImage", "[cmd][canvas-dag]")
{
  // Image-in-paint: the canvas reaches the draw through its paint, named by
  // the canvasSample after it.
  StreamBuilder b;
  b.canvasRangeDrawingPaintSampling(2, 1); // B samples A but records first
  b.canvasRange(1, {});
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{1, 2});
  CHECK_FALSE(s.hadCycle);
}

TEST_CASE(
    "independent canvases keep record order among themselves",
    "[cmd][canvas-dag]")
{
  StreamBuilder b;
  b.canvasRange(3, {});
  b.canvasRange(1, {5}); // samples a later writer
  b.canvasRange(4, {});
  b.canvasRange(5, {});
  auto s = b.schedule();
  // 5 must precede 1; 3 and 4 stay put relative to everyone they can.
  REQUIRE(s.order == std::vector<uint64_t>{3, 4, 5, 1});
}

namespace
{
struct FakeTarget : gpu::RenderTarget
{
  FakeTarget() : RenderTarget(8, 8) {}
};

// Logs canvas frame open order; canvas draws drop against the null renderer.
class OrderSink : public deferred_test::TestSink
{
public:
  std::vector<gpu::RenderCanvas*> opened;
  Renderer* beginCanvasContent(gpu::RenderCanvas* canvas, uint32_t) override
  {
    opened.push_back(canvas);
    return nullptr;
  }
};

rcp<gpu::RenderCanvas> fakeCanvas()
{
  auto canvas = make_rcp<gpu::RenderCanvas>(8, 8);
  canvas->setBacking(nullptr, make_rcp<FakeTarget>());
  return canvas;
}

// The image ids the recorder named with canvasSample, in record order.
std::vector<RenderHandle> canvasSamples(DeferredSession& session)
{
  std::vector<RenderHandle> ids;
  RenderCommandReader r(
      session.commandBuffer().commandBytes(),
      session.commandBuffer().blobBytes());
  uint8_t type;
  while (r.next(type))
  {
    auto cmd = static_cast<RenderCmd>(type);
    if (cmd == RenderCmd::canvasSample)
    {
      ids.push_back(r.read<ResIdPOD>().id);
    }
    else
    {
      r.skip(payloadSizeOf(cmd));
    }
  }
  return ids;
}
} // namespace

TEST_CASE(
    "replay opens the sampled canvas before its reader despite record "
    "order",
    "[cmd][canvas-dag]")
{
  DeferredSession session(rive::ore::ReplayCaps{});
  auto canvasA = fakeCanvas();
  auto canvasB = fakeCanvas();

  // B samples A but records first, exactly as a script may issue it.
  Renderer* b = session.beginCanvasContent(canvasB.get(), 0);
  b->drawImage(canvasA->renderImage(), {}, BlendMode::srcOver, 1.0f);
  session.endCanvasContent(canvasB.get());
  Renderer* a = session.beginCanvasContent(canvasA.get(), 0);
  auto paint = session.makeRenderPaint();
  auto path = session.makeEmptyRenderPath();
  a->drawPath(path.get(), paint.get());
  session.endCanvasContent(canvasA.get());
  session.closeOpenRange();

  auto frame = snapshotFrame(session);
  OrderSink sink;
  DeferredReplayer replayer;
  replayer.replayFrame(frame, sink);

  REQUIRE(sink.opened.size() == 2);
  CHECK(sink.opened[0] == canvasA.get());
  CHECK(sink.opened[1] == canvasB.get());
}

TEST_CASE(
    "replay opens a canvas before a reader whose paint samples it",
    "[cmd][canvas-dag]")
{
  DeferredSession session(rive::ore::ReplayCaps{});
  auto canvasA = fakeCanvas();
  auto canvasB = fakeCanvas();

  // B fills a path modulated by A's image but records first.
  auto modulated = session.makeRenderPaint();
  modulated->modulatedImage(
      canvasA->renderImage(),
      ImageSampler::LinearClamp(),
      Mat2D());
  auto path = session.makeEmptyRenderPath();
  Renderer* b = session.beginCanvasContent(canvasB.get(), 0);
  b->drawPath(path.get(), modulated.get());
  session.endCanvasContent(canvasB.get());
  Renderer* a = session.beginCanvasContent(canvasA.get(), 0);
  auto paint = session.makeRenderPaint();
  a->drawPath(path.get(), paint.get());
  session.endCanvasContent(canvasA.get());
  session.closeOpenRange();

  auto frame = snapshotFrame(session);
  OrderSink sink;
  DeferredReplayer replayer;
  replayer.replayFrame(frame, sink);

  REQUIRE(sink.opened.size() == 2);
  CHECK(sink.opened[0] == canvasA.get());
  CHECK(sink.opened[1] == canvasB.get());
}

TEST_CASE(
    "a canvas draw names the canvas its paint samples at the draw",
    "[cmd][canvas-dag]")
{
  DeferredSession session(rive::ore::ReplayCaps{});
  auto canvasA = fakeCanvas();
  auto canvasB = fakeCanvas();
  auto canvasC = fakeCanvas();
  auto paint = session.makeRenderPaint();
  auto path = session.makeEmptyRenderPath();
  auto modulate = [&](gpu::RenderCanvas* canvas) {
    paint->modulatedImage(
        canvas != nullptr ? canvas->renderImage() : nullptr,
        ImageSampler::LinearClamp(),
        Mat2D());
  };
  RenderHandle idA = session.canvases().imageDrawId(canvasA->renderImage());

  Renderer* b = session.beginCanvasContent(canvasB.get(), 0);
  modulate(canvasA.get());
  b->drawPath(path.get(), paint.get()); // samples A
  modulate(canvasC.get());              // repointed after the draw
  modulate(nullptr);
  b->drawPath(path.get(), paint.get()); // cleared, samples nothing
  session.endCanvasContent(canvasB.get());

  // Canvases replay before the screen, so a screen draw needs no edge.
  modulate(canvasA.get());
  session.screenRenderer()->drawPath(path.get(), paint.get());
  session.closeOpenRange();

  CHECK(canvasSamples(session) == std::vector<RenderHandle>{idA});
}

TEST_CASE("a canvas only frame still opens a screen frame", "[cmd][canvas-dag]")
{
  DeferredSession session(rive::ore::ReplayCaps{});
  auto canvas = fakeCanvas();

  Renderer* c = session.beginCanvasContent(canvas.get(), 0);
  auto paint = session.makeRenderPaint();
  auto path = session.makeEmptyRenderPath();
  c->drawPath(path.get(), paint.get());
  session.endCanvasContent(canvas.get());
  session.closeOpenRange();

  auto frame = snapshotFrame(session);
  OrderSink sink;
  DeferredReplayer replayer;
  replayer.replayFrame(frame, sink);

  REQUIRE(sink.opened.size() == 1);
  // The screen frame is where the host's clear and present live, so a frame
  // that only fills canvases still owes its target one.
  CHECK(sink.openedTargets() == 1);
}

TEST_CASE(
    "canvas content ended with saves open ends balanced and records "
    "nothing after",
    "[cmd][canvas-dag]")
{
  DeferredSession session(rive::ore::ReplayCaps{});
  auto canvas = fakeCanvas();
  auto paint = session.makeRenderPaint();
  auto path = session.makeEmptyRenderPath();

  Renderer* c = session.beginCanvasContent(canvas.get(), 0);
  c->save();
  c->save();
  session.endCanvasContent(canvas.get());
  // A drawer still holding the renderer, as an artboard draw visit whose
  // script ended the frame does.
  c->restore();
  c->drawPath(path.get(), paint.get());
  c->restore();
  // The canvas's next frame records again.
  CHECK(session.beginCanvasContent(canvas.get(), 0) == c);
  c->drawPath(path.get(), paint.get());
  session.endCanvasContent(canvas.get());
  session.closeOpenRange();

  auto stream = session.commandBuffer().commandBytes();
  CommandReader<RenderCmd> reader(stream, session.commandBuffer().blobBytes());
  int ranges = 0;
  int draws = 0;
  int depth = 0;
  RenderCmd type;
  while (reader.next(type))
  {
    if (type == RenderCmd::canvasContentBegin)
    {
      ranges++;
      depth = 0;
    }
    else if (type == RenderCmd::save)
    {
      depth++;
    }
    else if (type == RenderCmd::restore)
    {
      depth--;
      CHECK(depth >= 0);
    }
    else if (type == RenderCmd::drawPath)
    {
      draws++;
    }
    else if (type == RenderCmd::canvasContentEnd)
    {
      CHECK(depth == 0);
    }
    reader.skip(payloadSizeOf(type));
  }
  CHECK(ranges == 2);
  CHECK(draws == 1);
}

TEST_CASE(
    "a frame that only creates resources opens no screen frame",
    "[cmd][canvas-dag]")
{
  DeferredSession session(rive::ore::ReplayCaps{});
  // Creates land outside every renderer. Attributing them would open a
  // target that drew nothing, which is why they stay unattributed.
  auto paint = session.makeRenderPaint();
  auto path = session.makeEmptyRenderPath();
  session.closeOpenRange();

  auto frame = snapshotFrame(session);
  REQUIRE_FALSE(frame.commands.empty());

  OrderSink sink;
  DeferredReplayer replayer;
  replayer.replayFrame(frame, sink);

  CHECK(sink.openedTargets() == 0);
}

TEST_CASE(
    "a canvas that samples ore output replays after the run that wrote it",
    "[cmd][canvas-dag][ore]")
{
  StreamBuilder b;
  b.oreRun({}, {9}); // a GPU canvas written by Ore
  b.canvasRange(2, {9});
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{2});
  CHECK(sameRuns(s.earlyOre, {{0, 0, 16}}));
  CHECK(s.lateOreBegin == 16);
  CHECK_FALSE(s.orePinned);
}

TEST_CASE(
    "ore no canvas samples stays in the screen frame",
    "[cmd][canvas-dag][ore]")
{
  StreamBuilder b;
  b.oreRun({}, {9});
  b.canvasRange(2, {});
  auto s = b.schedule();
  CHECK(s.order == std::vector<uint64_t>{2});
  CHECK(s.earlyOre.empty());
  CHECK(s.lateOreBegin == 0);
}

TEST_CASE(
    "a sampled ore run pulls every earlier one ahead with it",
    "[cmd][canvas-dag][ore]")
{
  StreamBuilder b;
  b.oreRun({}, {8});
  b.oreRun({}, {9});
  b.oreRun({}, {7}); // after the sampled one, so it stays late
  b.canvasRange(2, {9});
  auto s = b.schedule();
  CHECK(sameRuns(s.earlyOre, {{0, 0, 32}}));
  CHECK(s.lateOreBegin == 32);
}

TEST_CASE(
    "an ore run replays after the canvases it samples",
    "[cmd][canvas-dag][ore]")
{
  StreamBuilder b;
  b.canvasRange(1, {});
  b.oreRun({1}, {9});
  b.canvasRange(2, {9});
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{1, 2});
  CHECK(sameRuns(s.earlyOre, {{1, 0, 16}}));
}

TEST_CASE(
    "a reader recorded before the ore run it samples still follows it",
    "[cmd][canvas-dag][ore]")
{
  StreamBuilder b;
  b.canvasRange(2, {9});
  b.oreRun({}, {9});
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{2});
  CHECK(sameRuns(s.earlyOre, {{0, 0, 16}}));
  CHECK_FALSE(s.hadCycle);
}

TEST_CASE(
    "a cycle keeps an ore run recorded just before its reader first",
    "[cmd][canvas-dag][ore]")
{
  StreamBuilder b;
  b.oreRun({2}, {9}); // closes at the very offset canvas 2 opens at
  b.canvasRange(2, {9});
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{2});
  CHECK(s.hadCycle);
  CHECK(sameRuns(s.earlyOre, {{0, 0, 16}}));
}

TEST_CASE(
    "nested masks run each combine between its canvases",
    "[cmd][canvas-dag][ore]")
{
  StreamBuilder b;
  b.canvasRange(11, {});    // outer coverage
  b.canvasRange(10, {});    // outer content, interrupted by the inner mask
  b.canvasRange(21, {});    // inner coverage
  b.canvasRange(20, {});    // inner content
  b.oreRun({20, 21}, {22}); // inner combine
  b.canvasRange(10, {22});  // outer content resumes and draws the inner result
  b.oreRun({10, 11}, {12}); // outer combine, only the screen samples it
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{11, 21, 20, 10});
  CHECK(sameRuns(s.earlyOre, {{3, 0, 16}}));
  CHECK(s.lateOreBegin == 16);
  CHECK_FALSE(s.hadCycle);
}

TEST_CASE(
    "a read between two writes keeps the frame's ore early",
    "[cmd][canvas-dag][ore]")
{
  StreamBuilder b;
  b.canvasRange(1, {});
  b.canvasRange(2, {1, 9}); // reads 1 between its two writes
  b.canvasRange(1, {});
  b.oreRun({}, {9});
  auto s = b.schedule();
  REQUIRE(s.order == std::vector<uint64_t>{1, 2});
  CHECK(sameRuns(s.earlyOre, {{1, 0, 16}}));
  CHECK(s.lateOreBegin == 16);
}

TEST_CASE(
    "a host target ore run ahead of a sampled one keeps ore late",
    "[cmd][canvas-dag][ore]")
{
  StreamBuilder b;
  b.oreRun({}, {}, true);
  b.oreRun({}, {9});
  b.canvasRange(2, {9});
  auto s = b.schedule();
  CHECK(s.orePinned);
  CHECK(s.earlyOre.empty());
  CHECK(s.lateOreBegin == 0);
}

#if defined(RIVE_DEFERRED_ORE) && defined(RIVE_CANVAS)
namespace
{
rcp<ore::BindGroup> sampleGroup(ore::Context& ore, ore::TextureView* view)
{
  ore::BindGroupLayoutEntry entry{};
  entry.binding = 0;
  entry.kind = ore::BindingKind::sampledTexture;
  ore::BindGroupLayoutDesc layoutDesc{};
  layoutDesc.entries = &entry;
  layoutDesc.entryCount = 1;
  rcp<ore::BindGroupLayout> layout = ore.makeBindGroupLayout(layoutDesc);
  REQUIRE(layout != nullptr);
  ore::BindGroupDesc::TexEntry tex{0, view};
  ore::BindGroupDesc groupDesc{};
  groupDesc.layout = layout.get();
  groupDesc.textures = &tex;
  groupDesc.textureCount = 1;
  return ore.makeBindGroup(groupDesc);
}

ore::RenderPassDesc passInto(ore::TextureView* view)
{
  ore::RenderPassDesc desc{};
  desc.colorAttachments[0].view = view;
  desc.colorCount = 1;
  return desc;
}
} // namespace

TEST_CASE(
    "an ore pass names the canvases it samples and writes",
    "[cmd][canvas-dag][ore]")
{
  DeferredSession session(ore::ReplayCaps{});
  auto& ore = session.oreContext();
  rcp<gpu::RenderCanvas> sampled = session.makeUnboundContentCanvas(4, 4);
  rcp<gpu::RenderCanvas> written = session.makeUnboundContentCanvas(4, 4);
  REQUIRE(sampled != nullptr);
  REQUIRE(written != nullptr);
  rcp<ore::TextureView> sampledView = ore.wrapCanvasTexture(sampled.get());
  rcp<ore::BindGroup> group = sampleGroup(ore, sampledView.get());
  REQUIRE(group != nullptr);
  rcp<ore::TextureView> writtenView = ore.wrapCanvasTexture(written.get());

  auto pass = ore.beginRenderPass(passInto(writtenView.get()));
  REQUIRE(pass != nullptr);
  pass->setBindGroup(0, group.get());
  CHECK(session.oreSegments().empty());
  pass->finish();

  REQUIRE(session.oreSegments().size() == 1);
  const DeferredOreSegment& seg = session.oreSegments()[0];
  auto idOf = [&](gpu::RenderCanvas* canvas) -> uint64_t {
    return session.canvases().imageDrawId(canvas->renderImage()) &
           kCanvasHandleMask;
  };
  CHECK(seg.begin == 0);
  CHECK(seg.end == ore.stream().commandBytes().size());
  CHECK(seg.reads == std::vector<uint64_t>{idOf(sampled.get())});
  CHECK(seg.writes == std::vector<uint64_t>{idOf(written.get())});
  CHECK_FALSE(seg.pinned);
}

TEST_CASE(
    "a nested ore pass shares its outer pass's run",
    "[cmd][canvas-dag][ore]")
{
  DeferredSession session(ore::ReplayCaps{});
  auto& ore = session.oreContext();
  rcp<gpu::RenderCanvas> outer = session.makeUnboundContentCanvas(4, 4);
  rcp<gpu::RenderCanvas> inner = session.makeUnboundContentCanvas(4, 4);
  rcp<ore::TextureView> outerView = ore.wrapCanvasTexture(outer.get());
  rcp<ore::TextureView> innerView = ore.wrapCanvasTexture(inner.get());

  auto outerPass = ore.beginRenderPass(passInto(outerView.get()));
  auto innerPass = ore.beginRenderPass(passInto(innerView.get()));
  innerPass->finish();
  CHECK(session.oreSegments().empty());
  outerPass->finish();

  REQUIRE(session.oreSegments().size() == 1);
  CHECK(session.oreSegments()[0].writes.size() == 2);
  CHECK(session.oreSegments()[0].end == ore.stream().commandBytes().size());
}

TEST_CASE(
    "a frame keeps the run of an ore pass left open",
    "[cmd][canvas-dag][ore]")
{
  DeferredSession session(ore::ReplayCaps{});
  auto& ore = session.oreContext();
  rcp<gpu::RenderCanvas> written = session.makeUnboundContentCanvas(4, 4);
  rcp<ore::TextureView> view = ore.wrapCanvasTexture(written.get());
  auto pass = ore.beginRenderPass(passInto(view.get()));
  REQUIRE(pass != nullptr);

  DeferredFrame frame = takeFrame(session);
  REQUIRE(frame.oreSegments.size() == 1);
  CHECK(frame.oreSegments[0].end == frame.oreCommands.size());
}
#endif
