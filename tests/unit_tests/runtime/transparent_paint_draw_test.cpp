/*
 * Copyright 2026 Rive
 *
 * A shape paint whose color, or every stop of whose gradient, is fully
 * transparent draws nothing, so Shape::draw doesn't hand it to the renderer.
 * Its visibility follows the color: when it shows again it draws again.
 */

#include <rive/artboard.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/gradient_stop.hpp>
#include <rive/shapes/paint/linear_gradient.hpp>
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

// A 10x10 rectangle with a fill and a stroke, each a solid color, plus a
// second fill with a two stop linear gradient.
struct PaintScene
{
  NoOpFactory factory;
  Artboard artboard{&factory};
  Fill* fill = nullptr;
  SolidColor* fillColor = nullptr;
  Stroke* stroke = nullptr;
  SolidColor* strokeColor = nullptr;
  Fill* gradientFill = nullptr;
  std::vector<GradientStop*> stops;

  PaintScene()
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
    fillColor = new SolidColor();
    fillColor->colorValue(0xFF336699);
    artboard.addObject(fillColor);
    fillColor->parentId(artboard.idOf(fill));

    stroke = new Stroke();
    stroke->thickness(2.0f);
    artboard.addObject(stroke);
    stroke->parentId(artboard.idOf(shape));
    strokeColor = new SolidColor();
    strokeColor->colorValue(0xFF000000);
    artboard.addObject(strokeColor);
    strokeColor->parentId(artboard.idOf(stroke));

    gradientFill = new Fill();
    artboard.addObject(gradientFill);
    gradientFill->parentId(artboard.idOf(shape));
    auto* gradient = new LinearGradient();
    gradient->endX(10.0f);
    artboard.addObject(gradient);
    gradient->parentId(artboard.idOf(gradientFill));
    for (float position : {0.0f, 1.0f})
    {
      auto* stop = new GradientStop();
      stop->colorValue(0xFFFF0000);
      stop->position(position);
      artboard.addObject(stop);
      stop->parentId(artboard.idOf(gradient));
      stops.push_back(stop);
    }

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

TEST_CASE("every visible paint draws", "[transparent-paint]")
{
  PaintScene scene;
  PaintRecorder drawn = scene.draw();
  CHECK(drawn.drew(scene.fill));
  CHECK(drawn.drew(scene.stroke));
  CHECK(drawn.drew(scene.gradientFill));
}

TEST_CASE("a fully transparent color doesn't draw", "[transparent-paint]")
{
  PaintScene scene;
  scene.fillColor->colorValue(0x00336699);
  PaintRecorder drawn = scene.draw();
  CHECK_FALSE(drawn.drew(scene.fill));
  // The shape's other paints are unaffected.
  CHECK(drawn.drew(scene.stroke));
  CHECK(drawn.drew(scene.gradientFill));

  SECTION("and draws again once it shows")
  {
    // A color change takes effect without an update: SolidColor recomputes
    // its visibility as the value changes.
    scene.fillColor->colorValue(0x01336699);
    CHECK(scene.draw().drew(scene.fill));
  }

  SECTION("for strokes too")
  {
    scene.strokeColor->colorValue(0x00000000);
    PaintRecorder strokeless = scene.draw();
    CHECK_FALSE(strokeless.drew(scene.stroke));
    scene.strokeColor->colorValue(0xFF000000);
    CHECK(scene.draw().drew(scene.stroke));
  }
}

TEST_CASE("a translucent color still draws", "[transparent-paint]")
{
  PaintScene scene;
  scene.fillColor->colorValue(0x01336699);
  CHECK(scene.draw().drew(scene.fill));
}

TEST_CASE(
    "a gradient with only transparent stops doesn't draw",
    "[transparent-paint]")
{
  PaintScene scene;
  for (auto* stop : scene.stops)
  {
    stop->colorValue(0x00FF0000);
  }
  // Gradients work out their visibility as they rebuild, in update().
  scene.artboard.advance(0.0f);
  CHECK_FALSE(scene.draw().drew(scene.gradientFill));
  CHECK(scene.draw().drew(scene.fill));

  SECTION("one visible stop is enough")
  {
    scene.stops.back()->colorValue(0x80FF0000);
    scene.artboard.advance(0.0f);
    CHECK(scene.draw().drew(scene.gradientFill));
  }
}

TEST_CASE("a hidden paint still doesn't draw", "[transparent-paint]")
{
  PaintScene scene;
  scene.fill->isVisible(false);
  CHECK_FALSE(scene.draw().drew(scene.fill));
  scene.fill->isVisible(true);
  CHECK(scene.draw().drew(scene.fill));
}
