/*
 * Copyright 2026 Rive
 */

// A host that composites through a fresh renderer (WebGL) must still see the
// draw clipped: the callers hand the host the original renderer so it can
// copy the clip onto the fresh one.

#include "rive/offscreen_raster.hpp"

#ifdef RIVE_CANVAS

#include "common/render_context_null.hpp"
#include "rive/artboard.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/paint_image.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape.hpp"
#include "utils/no_op_renderer.hpp"

#include <catch.hpp>
#include <memory>
#include <vector>

using namespace rive;

namespace
{
// Tracks transform, opacity and clips through save/restore, and records how
// many clips were active at each drawPath.
class ClipRecorder : public NoOpRenderer
{
public:
  std::vector<size_t> clipsAtDraw;

  void save() override { m_stack.push_back(m_stack.back()); }
  void restore() override
  {
    m_stack.pop_back();
    m_clips.resize(m_stack.back().clips);
  }
  void transform(const Mat2D& m) override
  {
    m_stack.back().matrix = m_stack.back().matrix * m;
  }
  void modulateOpacity(float opacity) override
  {
    m_stack.back().opacity *= opacity;
  }
  void clipPath(RenderPath* path) override
  {
    m_clips.resize(m_stack.back().clips);
    m_clips.push_back(path);
    m_stack.back().clips++;
  }
  void drawPath(RenderPath*, RenderPaint*) override
  {
    clipsAtDraw.push_back(m_stack.back().clips);
  }
  bool currentTransform(Mat2D* out) const override
  {
    *out = m_stack.back().matrix;
    return true;
  }
  bool currentModulatedOpacity(float* out) const override
  {
    *out = m_stack.back().opacity;
    return true;
  }
  void copyClipTo(Renderer* dst) const
  {
    for (size_t i = 0; i < m_stack.back().clips; ++i)
    {
      dst->clipPath(m_clips[i]);
    }
  }

private:
  struct State
  {
    Mat2D matrix;
    float opacity = 1.0f;
    size_t clips = 0;
  };
  std::vector<State> m_stack{1};
  std::vector<RenderPath*> m_clips;
};

// Hands back a fresh renderer to composite through, as WebGL's host does.
class FreshCompositeSession : public cmd::DeferredSession
{
public:
  using DeferredSession::DeferredSession;

  std::vector<std::unique_ptr<ClipRecorder>> composites;

  // Every renderer in these tests is a ClipRecorder.
  Renderer* compositeRenderer(Renderer* current) override
  {
    composites.push_back(std::make_unique<ClipRecorder>());
    static_cast<ClipRecorder*>(current)->copyClipTo(composites.back().get());
    return composites.back().get();
  }
};

Shape* addRect(
    Artboard& artboard,
    float x,
    float y,
    float width,
    float height,
    Fill** fillOut = nullptr)
{
  auto* shape = new Shape();
  artboard.addObject(shape);
  shape->parentId(0);
  shape->x(x);
  shape->y(y);
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

// A rectangle filled with an artboard paint that overhangs its host, as in a
// card whose dot pattern is cropped by a rounded clip.
struct PaintedScene
{
  std::unique_ptr<gpu::RenderContext>
      renderContext = RenderContextNULL::MakeContext();
  FreshCompositeSession session{rive::ore::ReplayCaps{}};
  std::unique_ptr<Artboard> source;
  std::unique_ptr<Artboard> host;
  std::unique_ptr<ArtboardInstance> instance;

  explicit PaintedScene(bool clipped)
  {
    session.bindRenderContext(renderContext.get());

    source = std::make_unique<Artboard>(&session);
    source->addObject(source.get());
    addRect(*source, 10, 10, 20, 20);
    REQUIRE(source->initialize() == StatusCode::Ok);
    source->width(20);
    source->height(20);

    host = std::make_unique<Artboard>(&session);
    host->addObject(host.get());
    Fill* fill = nullptr;
    Shape* painted = addRect(*host, 100, 100, 300, 300, &fill);
    auto* paint = new PaintImage();
    host->addObject(paint);
    paint->parentId(host->idOf(fill));
    if (clipped)
    {
      Shape* clipSource = addRect(*host, 100, 100, 150, 150);
      auto* clip = new ClippingShape();
      host->addObject(clip);
      clip->parentId(host->idOf(painted));
      clip->sourceId(host->idOf(clipSource));
    }
    REQUIRE(host->initialize() == StatusCode::Ok);
    host->width(200);
    host->height(200);
    // Isolates the ClippingShape from the artboard's own frame clip.
    host->clip(false);
    paint->referencedArtboard(source.get());

    instance = host->instance<ArtboardInstance>(&session);
    REQUIRE(instance != nullptr);
    instance->advance(0.0f);
  }

  // Draws one frame and returns the clips active at each draw through a
  // fresh composite renderer.
  std::vector<size_t> frame()
  {
    ClipRecorder screen;
    instance->drawInternal(&screen);
    session.resetFrame();
    std::vector<size_t> clips;
    for (auto& composite : session.composites)
    {
      clips.insert(
          clips.end(),
          composite->clipsAtDraw.begin(),
          composite->clipsAtDraw.end());
    }
    session.composites.clear();
    return clips;
  }
};
} // namespace

TEST_CASE(
    "an artboard paint under a clipping shape composites clipped",
    "[paint-artboard][composite-clip]")
{
  PaintedScene unclipped(/*clipped=*/false);
  CHECK(unclipped.frame() == std::vector<size_t>{0});

  PaintedScene clipped(/*clipped=*/true);
  CHECK(clipped.frame() == std::vector<size_t>{1});
}

TEST_CASE(
    "a cached composite through a fresh renderer keeps the clip",
    "[offscreen-raster][composite-clip]")
{
  auto renderContext = RenderContextNULL::MakeContext();
  FreshCompositeSession session{rive::ore::ReplayCaps{}};
  session.bindRenderContext(renderContext.get());
  auto clip = session.makeEmptyRenderPath();
  auto path = session.makeEmptyRenderPath();

  ClipRecorder screen;
  screen.save();
  screen.clipPath(clip.get());
  offscreen::RasterPlan plan;
  REQUIRE(offscreen::planRasterScale(&screen, 1.0f, &plan));
  plan.box = AABB(0, 0, 10, 10);

  SECTION("beginComposite")
  {
    auto placement = offscreen::beginComposite(
        &screen,
        &session,
        plan,
        plan.rasterScale);
    REQUIRE(placement.renderer != &screen);
    placement.renderer->drawPath(path.get(), nullptr);
    offscreen::endComposite(placement);
  }
  SECTION("beginPlacement")
  {
    auto placement = offscreen::beginPlacement(&screen, &session, plan);
    REQUIRE(placement.renderer != &screen);
    placement.renderer->drawPath(path.get(), nullptr);
    offscreen::endComposite(placement);
  }
  REQUIRE(session.composites.size() == 1);
  CHECK(session.composites[0]->clipsAtDraw == std::vector<size_t>{1});
  screen.restore();
}

#endif // RIVE_CANVAS
