/*
 * Copyright 2026 Rive
 */

// An artboard as a paint's image (PaintImage.artboardId). The source artboard
// is instanced once per host instance, advanced with it, and rasterized into
// an offscreen canvas whenever it changes; the canvas becomes the paint's
// modulated image.
//
// Fixtures are built in code: a host artboard with one filled rectangle whose
// Fill carries a PaintImage, pointed at a source artboard with its own
// rectangle. Recording goes through a DeferredSession bound to a GPU-free
// RenderContextNULL, the same harness the bitmap cache tests use.

#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/node.hpp"
#include "rive/shapes/paint/artboard_paint_source.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/paint_image.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_number_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"
#include "rive_file_reader.hpp"
#include "utils/lite_rtti.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/no_op_renderer.hpp"

#include <catch.hpp>
#include <memory>
#include <vector>

#ifdef RIVE_CANVAS
#include "common/render_context_null.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive_render_paint.hpp"
#endif

using namespace rive;

namespace
{
// A filled rectangle centred on its shape's origin. Returns the shape; the
// fill comes back through [fillOut] for callers that hang a PaintImage on it.
Shape* makeRect(
    Artboard& artboard,
    Node* parent,
    float width,
    float height,
    Fill** fillOut = nullptr)
{
  auto* shape = new Shape();
  artboard.addObject(shape);
  shape->parentId(artboard.idOf(parent));

  auto* rect = new rive::Rectangle();
  rect->width(width);
  rect->height(height);
  artboard.addObject(rect);
  rect->parentId(artboard.idOf(shape));

  auto* fill = new Fill();
  artboard.addObject(fill);
  fill->parentId(artboard.idOf(shape));

  auto* color = new SolidColor();
  color->colorValue(0xFFFFFFFF);
  artboard.addObject(color);
  color->parentId(artboard.idOf(fill));

  if (fillOut != nullptr)
  {
    *fillOut = fill;
  }
  return shape;
}

// A source artboard: one rectangle filling it.
struct Source
{
  Artboard artboard;
  Shape* shape = nullptr;

  Source(Factory* factory, float width, float height) : artboard(factory)
  {
    artboard.addObject(&artboard);
    shape = makeRect(artboard, &artboard, width, height);
    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.width(width);
    artboard.height(height);
    // Centred on the frame, so the rectangle covers the whole artboard.
    shape->x(width * 0.5f);
    shape->y(height * 0.5f);
    artboard.advance(0.0f);
  }
};

// A host artboard whose one rectangle is filled with a PaintImage. The
// PaintImage is pointed at [source] directly, which is what the importer's
// resolve does for a file.
struct Host
{
  Artboard artboard;
  PaintImage* paintImage = nullptr;

  Host(
      Factory* factory,
      Artboard* source,
      float width,
      float height,
      uint8_t sizeMode = 0)
      : artboard(factory)
  {
    artboard.addObject(&artboard);
    Fill* fill = nullptr;
    makeRect(artboard, &artboard, width, height, &fill);
    paintImage = new PaintImage();
    paintImage->imageSizeMode(sizeMode);
    artboard.addObject(paintImage);
    paintImage->parentId(artboard.idOf(fill));
    REQUIRE(artboard.initialize() == StatusCode::Ok);
    paintImage->referencedArtboard(source);
  }

  std::unique_ptr<ArtboardInstance> instance(Factory* factory)
  {
    auto out = artboard.instance<ArtboardInstance>(factory);
    REQUIRE(out != nullptr);
    out->advance(0.0f);
    return out;
  }
};

PaintImage* paintImageOf(ArtboardInstance* artboard)
{
  for (Core* object : artboard->objects())
  {
    if (object != nullptr && object->is<PaintImage>())
    {
      return object->as<PaintImage>();
    }
  }
  return nullptr;
}

#ifdef RIVE_CANVAS
// Logs every raster the paint asks for, with the canvas size it asked for.
class CountingSession : public cmd::DeferredSession
{
public:
  using DeferredSession::DeferredSession;

  std::vector<std::pair<uint32_t, uint32_t>> rasters;

  Renderer* beginCanvasContent(gpu::RenderCanvas* canvas, uint32_t clearColor)
      override
  {
    rasters.push_back({canvas->width(), canvas->height()});
    return DeferredSession::beginCanvasContent(canvas, clearColor);
  }
};

struct Harness
{
  std::unique_ptr<gpu::RenderContext>
      renderContext = RenderContextNULL::MakeContext();
  CountingSession session{rive::ore::ReplayCaps{}};

  explicit Harness(bool canRaster = true)
  {
    session.bindRenderContext(canRaster ? renderContext.get() : nullptr);
  }

  // Draws one frame of [artboard] at [scale] device pixels per unit.
  void draw(ArtboardInstance* artboard, float scale = 1.0f)
  {
    Renderer* screen = session.screenRenderer();
    screen->save();
    screen->transform(Mat2D::fromScale(scale, scale));
    artboard->drawInternal(screen);
    screen->restore();
    session.resetFrame();
  }
};
#endif
} // namespace

TEST_CASE(
    "each host instance plays its own instance of the source",
    "[paint-artboard]")
{
  NoOpFactory factory;
  Source source(&factory, 100.0f, 50.0f);
  Host host(&factory, &source.artboard, 10.0f, 10.0f);

  // The file level host is never drawn, so it instances nothing.
  CHECK(host.paintImage->artboardSource() == nullptr);
  CHECK(host.paintImage->sourceArtboard() == &source.artboard);

  auto a = host.instance(&factory);
  auto b = host.instance(&factory);
  PaintImage* paintA = paintImageOf(a.get());
  PaintImage* paintB = paintImageOf(b.get());
  REQUIRE(paintA != nullptr);
  REQUIRE(paintB != nullptr);
  REQUIRE(paintA->artboardSource() != nullptr);
  REQUIRE(paintB->artboardSource() != nullptr);
  CHECK(
      paintA->artboardSource()->artboard() !=
      paintB->artboardSource()->artboard());
  // Both still point at the file level source, never at an instance.
  CHECK(paintA->sourceArtboard() == &source.artboard);
  CHECK(paintA->artboardSource()->artboard()->isInstance());
  // An artboard authored with no scene still gets the runtime's
  // auto-generated (empty) state machine, which is what plays it.
  CHECK(paintA->artboardSource()->hasStateMachine());
  CHECK_FALSE(paintA->artboardSource()->hasAnimation());
  // "Original" maps from the artboard's own size.
  AABB box = paintA->artboardSource()->bounds();
  CHECK(box.width() == Approx(100.0f));
  CHECK(box.height() == Approx(50.0f));
}

TEST_CASE(
    "a change inside the source marks the host changed",
    "[paint-artboard]")
{
  NoOpFactory factory;
  Source source(&factory, 100.0f, 50.0f);
  Host host(&factory, &source.artboard, 10.0f, 10.0f);
  auto instance = host.instance(&factory);
  PaintImage* paint = paintImageOf(instance.get());
  REQUIRE(paint->artboardSource() != nullptr);

  // Settle: nothing pending on either side.
  instance->advance(0.0f);
  rive::NoOpRenderer renderer;
  instance->drawInternal(&renderer);
  CHECK_FALSE(instance->didChange());

  // Move something inside the paint's own instance of the source.
  ArtboardInstance* inner = paint->artboardSource()->artboard();
  Shape* innerShape = nullptr;
  for (Core* object : inner->objects())
  {
    if (object != nullptr && object->is<Shape>())
    {
      innerShape = object->as<Shape>();
    }
  }
  REQUIRE(innerShape != nullptr);
  innerShape->x(innerShape->x() + 5.0f);

  instance->advance(0.0f);
  // The host has to draw the frame that re-rasterizes the source.
  CHECK(instance->didChange());
  // And the update pass consumed the source's dirt.
  CHECK_FALSE(paint->artboardSource()->needsUpdate());
}

TEST_CASE("an artboard that paints itself stops nesting", "[paint-artboard]")
{
  // The importer cuts this cycle (see below); the depth guard is the backstop
  // for one the editor's artboard splice would never re-check. Without it,
  // instancing recurses until the stack overflows.
  NoOpFactory factory;
  Host host(&factory, nullptr, 10.0f, 10.0f);
  host.paintImage->referencedArtboard(&host.artboard);

  auto instance = host.instance(&factory);
  int depth = 0;
  ArtboardInstance* level = instance.get();
  while (level != nullptr)
  {
    PaintImage* paint = paintImageOf(level);
    REQUIRE(paint != nullptr);
    ArtboardPaintSource* source = paint->artboardSource();
    level = source != nullptr ? source->artboard() : nullptr;
    depth++;
    REQUIRE(depth < 100);
  }
  CHECK(depth > 1);
  CHECK(depth <= 17);
}

TEST_CASE("the importer cuts paint artboard cycles", "[paint-artboard]")
{
  NoOpFactory factory;
  Host a(&factory, nullptr, 10.0f, 10.0f);
  Host b(&factory, nullptr, 10.0f, 10.0f);
  Host self(&factory, nullptr, 10.0f, 10.0f);
  Source plain(&factory, 10.0f, 10.0f);
  Host user(&factory, nullptr, 10.0f, 10.0f);

  // a <-> b, self -> self, and user -> plain (no cycle).
  a.paintImage->artboardId(1);
  b.paintImage->artboardId(0);
  self.paintImage->artboardId(2);
  user.paintImage->artboardId(3);

  Backboard backboard;
  BackboardImporter importer(&backboard);
  importer.addArtboard(&a.artboard);
  importer.addArtboard(&b.artboard);
  importer.addArtboard(&self.artboard);
  importer.addArtboard(&plain.artboard);
  importer.addArtboard(&user.artboard);
  importer.addArtboardReferencer(a.paintImage);
  importer.addArtboardReferencer(b.paintImage);
  importer.addArtboardReferencer(self.paintImage);
  importer.addArtboardReferencer(user.paintImage);
  REQUIRE(importer.resolve() == StatusCode::Ok);

  CHECK(a.paintImage->sourceArtboard() == nullptr);
  CHECK(b.paintImage->sourceArtboard() == nullptr);
  CHECK(self.paintImage->sourceArtboard() == nullptr);
  CHECK(user.paintImage->sourceArtboard() == &plain.artboard);
}

TEST_CASE("ArtboardReferencer::from finds a paint image", "[paint-artboard]")
{
  // File::replaceArtboard re-points referencers through this switch; one it
  // misses keeps a pointer to the artboard the splice deleted.
  NoOpFactory factory;
  Host host(&factory, nullptr, 10.0f, 10.0f);
  CHECK(ArtboardReferencer::from(host.paintImage) == host.paintImage);
  CHECK(AdvancingComponent::from(host.paintImage) == host.paintImage);
}

namespace
{
// assets/paint_artboard_bind.riv, generated by
// rive_core/test/generate_paint_artboard_bind_riv_test.dart: "Host" fills a
// rectangle with "Source", whose red half has its x bound to view model
// number "redX". Both artboards use view model "vm". The red half is authored
// at x = 25 and the default instance sets redX to 30.
constexpr const char* kBindRiv = "assets/paint_artboard_bind.riv";

// The source's red half inside [paint]'s own instance: the shape authored at
// x = 25 (the blue half sits at 75).
Shape* boundShapeOf(PaintImage* paint)
{
  for (Core* object : paint->artboardSource()->artboard()->objects())
  {
    if (object != nullptr && object->is<Shape>() &&
        object->as<Shape>()->x() == 25.0f)
    {
      return object->as<Shape>();
    }
  }
  return nullptr;
}
} // namespace

TEST_CASE(
    "the source binds to the data context of the paint's artboard",
    "[paint-artboard]")
{
  auto file = ReadRiveFile(kBindRiv);
  auto host = file->artboardNamed("Host");
  REQUIRE(host != nullptr);
  PaintImage* paint = paintImageOf(host.get());
  REQUIRE(paint != nullptr);
  REQUIRE(paint->artboardSource() != nullptr);
  Shape* red = boundShapeOf(paint);
  REQUIRE(red != nullptr);

  auto viewModel = file->viewModelByName("vm");
  REQUIRE(viewModel != nullptr);
  auto instance = viewModel->createDefaultInstance();
  REQUIRE(instance != nullptr);
  auto redX = instance->propertyNumber("redX");
  REQUIRE(redX != nullptr);
  redX->value(60.0f);

  // Bound to the host only: the context reaches the source through the
  // paint, as a nested artboard with no bind path inherits its host's.
  host->bindViewModelInstance(instance->instance());
  host->advance(0.0f);
  CHECK(red->x() == Approx(60.0f));

  // A later change flows through, and marks the host changed so a host
  // gated on didChange() draws the frame that re-rasterizes the source.
  rive::NoOpRenderer renderer;
  host->drawInternal(&renderer);
  CHECK_FALSE(host->didChange());
  redX->value(75.0f);
  host->advance(0.0f);
  CHECK(red->x() == Approx(75.0f));
  CHECK(host->didChange());
}

TEST_CASE("an unbound host leaves the source as authored", "[paint-artboard]")
{
  auto file = ReadRiveFile(kBindRiv);
  auto host = file->artboardNamed("Host");
  REQUIRE(host != nullptr);
  PaintImage* paint = paintImageOf(host.get());
  REQUIRE(paint != nullptr);
  REQUIRE(paint->artboardSource() != nullptr);
  Shape* red = boundShapeOf(paint);
  REQUIRE(red != nullptr);
  host->advance(0.0f);
  CHECK(red->x() == Approx(25.0f));
}

TEST_CASE(
    "only a paint with an artboard source hosts an artboard",
    "[paint-artboard]")
{
  // Every host keeps the artboard's list rows from going quiet, so an image
  // paint must not become one -- nor report itself busy.
  NoOpFactory factory;
  Source source(&factory, 100.0f, 50.0f);
  Host imageOnly(&factory, nullptr, 10.0f, 10.0f);
  Host artboardPaint(&factory, &source.artboard, 10.0f, 10.0f);

  auto plain = imageOnly.instance(&factory);
  auto painted = artboardPaint.instance(&factory);
  PaintImage* plainPaint = paintImageOf(plain.get());
  PaintImage* artboardPaintImage = paintImageOf(painted.get());
  CHECK(plainPaint->quietState() == AdvancingComponent::QuietState::quiet);
  CHECK(
      artboardPaintImage->quietState() ==
      AdvancingComponent::QuietState::never);
  CHECK(plainPaint->artboardCount() == 0);
  CHECK(artboardPaintImage->artboardCount() == 1);
  // The paint is the instance's host, so changes walk up to the artboard
  // the paint is in.
  CHECK(
      artboardPaintImage->artboardSource()->artboard()->host() ==
      artboardPaintImage);
  CHECK(artboardPaintImage->parentArtboard() == painted.get());
}

#ifdef RIVE_CANVAS
TEST_CASE("a bound change re-rasterizes the source", "[paint-artboard]")
{
  Harness h;
  auto file = ReadRiveFile(kBindRiv, &h.session);
  auto host = file->artboardNamed("Host");
  REQUIRE(host != nullptr);
  auto instance = file->viewModelByName("vm")->createDefaultInstance();
  REQUIRE(instance != nullptr);
  host->bindViewModelInstance(instance->instance());
  host->advance(0.0f);
  h.draw(host.get());
  REQUIRE(h.session.rasters.size() == 1);

  // Nothing changed: the raster is reused.
  host->advance(0.016f);
  h.draw(host.get());
  CHECK(h.session.rasters.size() == 1);

  instance->propertyNumber("redX")->value(75.0f);
  host->advance(0.016f);
  h.draw(host.get());
  CHECK(h.session.rasters.size() == 2);
}

TEST_CASE("a static source rasterizes once", "[paint-artboard]")
{
  Harness h;
  Source source(&h.session, 100.0f, 50.0f);
  Host host(&h.session, &source.artboard, 10.0f, 10.0f);
  auto instance = host.instance(&h.session);
  PaintImage* paint = paintImageOf(instance.get());
  REQUIRE(paint->artboardSource() != nullptr);

  h.draw(instance.get());
  REQUIRE(h.session.rasters.size() == 1);
  CHECK(paint->artboardSource()->rasterCount() == 1);

  for (int i = 0; i < 3; i++)
  {
    instance->advance(0.016f);
    h.draw(instance.get());
  }
  CHECK(h.session.rasters.size() == 1);
}

TEST_CASE("a changed source re-rasterizes on the next draw", "[paint-artboard]")
{
  Harness h;
  Source source(&h.session, 100.0f, 50.0f);
  Host host(&h.session, &source.artboard, 10.0f, 10.0f);
  auto instance = host.instance(&h.session);
  PaintImage* paint = paintImageOf(instance.get());
  REQUIRE(paint->artboardSource() != nullptr);
  h.draw(instance.get());
  REQUIRE(h.session.rasters.size() == 1);

  ArtboardInstance* inner = paint->artboardSource()->artboard();
  for (Core* object : inner->objects())
  {
    if (object != nullptr && object->is<Shape>())
    {
      object->as<Shape>()->x(10.0f);
    }
  }
  instance->advance(0.016f);
  h.draw(instance.get());
  CHECK(h.session.rasters.size() == 2);

  // Settles again once nothing moves.
  instance->advance(0.016f);
  h.draw(instance.get());
  CHECK(h.session.rasters.size() == 2);
}

TEST_CASE(
    "the raster is sized from how the source is viewed",
    "[paint-artboard]")
{
  SECTION("fill stretches the source over the shape")
  {
    // A 100x50 source over a 1000x500 shape at 1x needs 10 texels per
    // source unit: 1000x500, bucketed to 1024x512.
    Harness h;
    Source source(&h.session, 100.0f, 50.0f);
    Host host(&h.session, &source.artboard, 1000.0f, 500.0f);
    auto instance = host.instance(&h.session);
    h.draw(instance.get());
    REQUIRE(h.session.rasters.size() == 1);
    CHECK(h.session.rasters[0].first == 1024);
    CHECK(h.session.rasters[0].second == 512);
  }
  SECTION("original keeps the source's own size")
  {
    // One source unit per shape unit, whatever the shape: 100x50 at 1x,
    // bucketed to 128x64.
    Harness h;
    Source source(&h.session, 100.0f, 50.0f);
    Host host(
        &h.session,
        &source.artboard,
        1000.0f,
        500.0f,
        /*sizeMode=*/1);
    auto instance = host.instance(&h.session);
    h.draw(instance.get());
    REQUIRE(h.session.rasters.size() == 1);
    CHECK(h.session.rasters[0].first == 128);
    CHECK(h.session.rasters[0].second == 64);
  }
  SECTION("device scale raises the texel count")
  {
    Harness h;
    Source source(&h.session, 100.0f, 50.0f);
    Host host(
        &h.session,
        &source.artboard,
        1000.0f,
        500.0f,
        /*sizeMode=*/1);
    auto instance = host.instance(&h.session);
    h.draw(instance.get(), 4.0f);
    REQUIRE(h.session.rasters.size() == 1);
    CHECK(h.session.rasters[0].first == 448);
    CHECK(h.session.rasters[0].second == 256);
  }
  SECTION("tiling needs fewer texels per tile")
  {
    Harness h;
    Source source(&h.session, 100.0f, 50.0f);
    Host host(&h.session, &source.artboard, 1000.0f, 500.0f);
    auto instance = host.instance(&h.session);
    PaintImage* paint = paintImageOf(instance.get());
    paint->imageScaleX(0.25f);
    paint->imageScaleY(0.25f);
    h.draw(instance.get());
    REQUIRE(h.session.rasters.size() == 1);
    CHECK(h.session.rasters[0].first == 256);
    CHECK(h.session.rasters[0].second == 128);
  }
  SECTION("the cap shrinks both axes together")
  {
    Harness h;
    Source source(&h.session, 100.0f, 50.0f);
    Host host(&h.session, &source.artboard, 8000.0f, 4000.0f);
    auto instance = host.instance(&h.session);
    h.draw(instance.get());
    REQUIRE(h.session.rasters.size() == 1);
    CHECK(h.session.rasters[0].first == 2048);
    CHECK(h.session.rasters[0].second == 1024);
  }
  SECTION("a zoom past what a uint32_t holds still caps")
  {
    // 64 source units at 2^26 + 16 texels each is 2^32 + 1024, which a
    // uint32_t cannot hold. Converted before the cap, x86 wrapped it to
    // 1024, under the 2^31 + 512 the other axis asked for, so the raster
    // came out with its axes inverted.
    Harness h;
    Source source(&h.session, 64.0f, 32.0f);
    Host host(
        &h.session,
        &source.artboard,
        64.0f,
        32.0f,
        /*sizeMode=*/1);
    auto instance = host.instance(&h.session);
    h.draw(instance.get(), 67108880.0f);
    REQUIRE(h.session.rasters.size() == 1);
    CHECK(h.session.rasters[0].first == 2048);
    CHECK(h.session.rasters[0].second == 1024);
  }
}

TEST_CASE("zooming in re-rasterizes, zooming out waits", "[paint-artboard]")
{
  Harness h;
  Source source(&h.session, 100.0f, 50.0f);
  Host host(&h.session, &source.artboard, 100.0f, 50.0f);
  auto instance = host.instance(&h.session);
  h.draw(instance.get(), 1.0f);
  REQUIRE(h.session.rasters.size() == 1);
  CHECK(h.session.rasters.back().first == 128);

  // Growth is immediate: a raster smaller than it is viewed blurs.
  h.draw(instance.get(), 4.0f);
  REQUIRE(h.session.rasters.size() == 2);
  CHECK(h.session.rasters.back().first == 448);

  // Shrinking holds the larger texture until the streak runs out, and
  // re-rasterizes nothing meanwhile: the content is stretched over the
  // whole canvas, so a held allocation still maps exactly.
  h.draw(instance.get(), 1.0f);
  CHECK(h.session.rasters.size() == 2);
}

TEST_CASE(
    "a host that cannot raster leaves the paint without an image",
    "[paint-artboard]")
{
  Harness h(/*canRaster=*/false);
  Source source(&h.session, 100.0f, 50.0f);
  Host host(&h.session, &source.artboard, 10.0f, 10.0f);
  auto instance = host.instance(&h.session);
  PaintImage* paint = paintImageOf(instance.get());
  REQUIRE(paint->artboardSource() != nullptr);
  h.draw(instance.get());
  CHECK(h.session.rasters.empty());
  CHECK(paint->artboardSource()->rasterCount() == 0);
}

namespace
{
// Replays a recorded frame against a null-device context, backing canvases
// for real, so the replayed paints can be inspected.
class NullReplaySink : public cmd::DeferredFrameSink
{
public:
  explicit NullReplaySink(gpu::RenderContext* ctx) : m_ctx(ctx)
  {
    m_screenTarget = m_ctx->static_impl_cast<RenderContextNULL>()
                         ->makeRenderTarget(256, 256);
  }

  Factory* factory() override { return m_ctx; }
  ore::Context* oreContext() override { return nullptr; }
  gpu::RenderContext* renderContext() override { return m_ctx; }

  Renderer* beginScreenFrame(uint64_t) override
  {
    m_ctx->beginFrame({.renderTargetWidth = 256, .renderTargetHeight = 256});
    m_screen = std::make_unique<RiveRenderer>(m_ctx);
    return m_screen.get();
  }
  void flushScreen()
  {
    if (m_screen != nullptr)
    {
      m_ctx->flush({.renderTarget = m_screenTarget.get()});
      m_screen = nullptr;
    }
  }
  Renderer* beginCanvasContent(gpu::RenderCanvas* canvas, uint32_t) override
  {
    m_canvasTarget = ref_rcp(canvas->renderTarget());
    m_ctx->beginFrame(
        {.renderTargetWidth = canvas->width(),
         .renderTargetHeight = canvas->height()});
    m_canvas = std::make_unique<RiveRenderer>(m_ctx);
    return m_canvas.get();
  }
  void endCanvasContent() override
  {
    m_ctx->flush({.renderTarget = m_canvasTarget.get()});
    m_canvas = nullptr;
    m_canvasTarget = nullptr;
  }

private:
  gpu::RenderContext* m_ctx;
  rcp<gpu::RenderTarget> m_screenTarget;
  rcp<gpu::RenderTarget> m_canvasTarget;
  std::unique_ptr<RiveRenderer> m_screen;
  std::unique_ptr<RiveRenderer> m_canvas;
};
} // namespace

TEST_CASE(
    "a paint samples its canvas on the first replayed frame",
    "[paint-artboard]")
{
  // The paint's modulated image replays in the resources pass, ahead of the
  // draws pass that backs a canvas. The replay backs a sampled canvas when
  // it hands out its image; without that the paint captured a null texture
  // and the first frame (and every frame after a resize) drew no image.
  Harness h;
  NullReplaySink sink(h.renderContext.get());
  Source source(&h.session, 100.0f, 50.0f);
  Host host(&h.session, &source.artboard, 10.0f, 10.0f);
  auto instance = host.instance(&h.session);

  Renderer* screen = h.session.screenRenderer();
  instance->drawInternal(screen);
  cmd::DeferredFrame frame = cmd::snapshotFrame(h.session);
  h.session.resetFrame();
  REQUIRE(h.session.rasters.size() == 1);

  cmd::DeferredReplayer replayer;
  replayer.replayFrame(frame, sink);
  sink.flushScreen();

  int textured = 0;
  for (const rcp<RenderPaint>& paint : replayer.table().paints.objects)
  {
    if (paint == nullptr)
    {
      continue;
    }
    auto* rivePaint = lite_rtti_cast<RiveRenderPaint*>(paint.get());
    if (rivePaint != nullptr && rivePaint->getImageTexture() != nullptr)
    {
      textured++;
    }
  }
  CHECK(textured == 1);
}
#endif
