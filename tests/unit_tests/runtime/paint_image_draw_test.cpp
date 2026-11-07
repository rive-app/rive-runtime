/*
 * Copyright 2026 Rive
 *
 * A shape paint with a PaintImage child modulates its color with the image.
 * Until that image resolves (none picked, not decoded yet, cleared, or an
 * artboard source that can't raster) the paint draws nothing, rather than
 * its bare color.
 */

#include <rive/artboard.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/paint_image.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/stroke.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>

#include <catch.hpp>
#include <algorithm>
#include <vector>

using namespace rive;

namespace
{
// Remembers the paint of every path it's asked to draw.
class PaintRecorder : public NoOpRenderer
{
public:
  std::vector<RenderPaint*> paints;

  void drawPath(RenderPath*, RenderPaint* paint) override
  {
    paints.push_back(paint);
  }

  bool drew(ShapePaint* shapePaint) const
  {
    return std::find(paints.begin(), paints.end(), shapePaint->renderPaint()) !=
           paints.end();
  }
};

class TestImage : public RenderImage
{};

// A 10x10 rectangle with a fill carrying a PaintImage, and a plain stroke.
struct ImageScene
{
  NoOpFactory factory;
  rcp<ImageAsset> asset = make_rcp<ImageAsset>();
  Artboard artboard{&factory};
  Fill* fill = nullptr;
  PaintImage* paintImage = nullptr;
  Stroke* stroke = nullptr;

  ImageScene()
  {
    artboard.addObject(&artboard);
    auto* shape = new Shape();
    artboard.addObject(shape);

    auto* rect = new Rectangle();
    rect->width(10.0f);
    rect->height(10.0f);
    artboard.addObject(rect);
    rect->parentId(artboard.idOf(shape));

    fill = new Fill();
    artboard.addObject(fill);
    fill->parentId(artboard.idOf(shape));
    auto* fillColor = new SolidColor();
    fillColor->colorValue(0xFFFFFFFF);
    artboard.addObject(fillColor);
    fillColor->parentId(artboard.idOf(fill));
    paintImage = new PaintImage();
    artboard.addObject(paintImage);
    paintImage->parentId(artboard.idOf(fill));

    stroke = new Stroke();
    stroke->thickness(2.0f);
    artboard.addObject(stroke);
    stroke->parentId(artboard.idOf(shape));
    auto* strokeColor = new SolidColor();
    strokeColor->colorValue(0xFF000000);
    artboard.addObject(strokeColor);
    strokeColor->parentId(artboard.idOf(stroke));

    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.advance(0.0f);
  }

  PaintRecorder draw()
  {
    PaintRecorder recorder;
    artboard.draw(&recorder);
    return recorder;
  }
};
} // namespace

TEST_CASE("a paint image without an asset doesn't draw", "[paint-image]")
{
  ImageScene scene;
  PaintRecorder drawn = scene.draw();
  CHECK_FALSE(drawn.drew(scene.fill));
  // The shape's other paints are unaffected.
  CHECK(drawn.drew(scene.stroke));
}

TEST_CASE("a paint image draws once its image decodes", "[paint-image]")
{
  ImageScene scene;
  // Assigned, but the bytes haven't decoded yet.
  scene.paintImage->setAsset(scene.asset);
  CHECK_FALSE(scene.draw().drew(scene.fill));

  scene.asset->renderImage(make_rcp<TestImage>());
  CHECK(scene.draw().drew(scene.fill));

  SECTION("and stops drawing once the image is cleared")
  {
    scene.asset->renderImage(nullptr);
    PaintRecorder drawn = scene.draw();
    CHECK_FALSE(drawn.drew(scene.fill));
    CHECK(drawn.drew(scene.stroke));
  }
}

TEST_CASE("an artboard source that can't raster doesn't draw", "[paint-image]")
{
  ImageScene scene;
  scene.paintImage->setAsset(scene.asset);
  scene.asset->renderImage(make_rcp<TestImage>());
  REQUIRE(scene.draw().drew(scene.fill));

  // An artboard source wins over the asset whenever it resolves. This one
  // never instances (the scene's artboard isn't an instance), and a no-op
  // factory has no canvas host to raster on anyway.
  Artboard source(&scene.factory);
  source.addObject(&source);
  REQUIRE(source.initialize() == StatusCode::Ok);
  scene.paintImage->referencedArtboard(&source);
  PaintRecorder drawn = scene.draw();
  CHECK_FALSE(drawn.drew(scene.fill));
  CHECK(drawn.drew(scene.stroke));
  scene.paintImage->referencedArtboard(nullptr);
}
