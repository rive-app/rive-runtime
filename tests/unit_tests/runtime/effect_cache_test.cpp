#include <rive/artboard.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/paint/dash.hpp>
#include <rive/shapes/paint/dash_path.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/stroke.hpp>
#include <rive/shapes/paint/trim_path.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>
#include <catch.hpp>

using namespace rive;

namespace
{
NoOpFactory gFactory;

struct StrokedRectangle
{
  Artboard artboard{&gFactory};
  Rectangle* rectangle = nullptr;
  Stroke* stroke = nullptr;

  StrokedRectangle()
  {
    artboard.addObject(&artboard);
    auto* shape = new Shape();
    artboard.addObject(shape);
    shape->parentId(artboard.idOf(&artboard));

    rectangle = new Rectangle();
    rectangle->width(100.0f);
    rectangle->height(100.0f);
    artboard.addObject(rectangle);
    rectangle->parentId(artboard.idOf(shape));

    stroke = new Stroke();
    stroke->thickness(4.0f);
    artboard.addObject(stroke);
    stroke->parentId(artboard.idOf(shape));

    // A paint with nothing to show defers its effects.
    auto* color = new SolidColor();
    color->colorValue(0xFF00FF00);
    artboard.addObject(color);
    color->parentId(artboard.idOf(stroke));
  }

  TrimPath* addTrim(float end)
  {
    auto* trim = new TrimPath();
    trim->modeValue(static_cast<uint32_t>(TrimPathMode::sequential));
    trim->start(0.0f);
    trim->end(end);
    artboard.addObject(trim);
    trim->parentId(artboard.idOf(stroke));
    return trim;
  }

  DashPath* addDash(float length)
  {
    auto* dashPath = new DashPath();
    artboard.addObject(dashPath);
    dashPath->parentId(artboard.idOf(stroke));
    auto* dash = new Dash();
    dash->length(length);
    artboard.addObject(dash);
    dash->parentId(artboard.idOf(dashPath));
    return dashPath;
  }

  // Draws too, so the last effect gets a render path the way it does in a
  // player.
  void frame()
  {
    NoOpRenderer renderer;
    artboard.advance(0.0f);
    artboard.draw(&renderer);
  }
};
} // namespace

TEST_CASE(
    "an effect before the last is computed once until invalidated",
    "[effects]")
{
  StrokedRectangle scene;
  auto* first = scene.addTrim(0.9f);
  auto* last = scene.addTrim(0.5f);
  REQUIRE(scene.artboard.initialize() == StatusCode::Ok);
  scene.frame();
  CHECK(first->computeCount == 1);
  CHECK(last->computeCount == 1);

  // Only the last effect changes, so only it computes again.
  for (int i = 1; i <= 10; i++)
  {
    last->end(0.5f + i * 0.01f);
    scene.frame();
  }
  CHECK(first->computeCount == 1);
  CHECK(last->computeCount == 11);

  // An uneventful frame computes nothing.
  scene.frame();
  CHECK(first->computeCount == 1);
  CHECK(last->computeCount == 11);

  // Invalidating the first effect invalidates the ones after it.
  first->end(0.8f);
  scene.frame();
  CHECK(first->computeCount == 2);
  CHECK(last->computeCount == 12);

  // So does a change to the source path.
  scene.rectangle->width(120.0f);
  scene.frame();
  CHECK(first->computeCount == 3);
  CHECK(last->computeCount == 13);
}

TEST_CASE(
    "a dash before a trim is computed once until invalidated",
    "[effects]")
{
  StrokedRectangle scene;
  auto* dash = scene.addDash(10.0f);
  auto* trim = scene.addTrim(0.5f);
  REQUIRE(scene.artboard.initialize() == StatusCode::Ok);
  scene.frame();
  CHECK(dash->computeCount == 1);
  CHECK(trim->computeCount == 1);

  for (int i = 1; i <= 5; i++)
  {
    trim->end(0.5f + i * 0.05f);
    scene.frame();
  }
  CHECK(dash->computeCount == 1);
  CHECK(trim->computeCount == 6);

  dash->offset(3.0f);
  scene.frame();
  CHECK(dash->computeCount == 2);
  CHECK(trim->computeCount == 7);
}
