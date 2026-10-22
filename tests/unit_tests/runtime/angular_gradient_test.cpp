/*
 * Copyright 2026 Rive
 *
 * The sweep is placed by the gradient's own transform alone, so a change to
 * any of those properties has to rebuild the shader. Animations (and so a
 * nested artboard playing them) reach the properties through the generated
 * setters only, with nothing else to mark the gradient dirty.
 */

#include <rive/artboard.hpp>
#include <rive/math/mat2d.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/paint/angular_gradient.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/gradient_stop.hpp>
#include <utils/no_op_factory.hpp>

#include <catch.hpp>

using namespace rive;

namespace
{
// NoOpFactory's paints discard everything. This one keeps the last shader
// transform pushed onto it and counts the shaders built.
class ShaderTransformPaint : public RenderPaint
{
public:
  Mat2D transform;
  int shaderWrites = 0;

  void style(RenderPaintStyle) override {}
  void color(ColorInt) override {}
  void thickness(float) override {}
  void join(StrokeJoin) override {}
  void cap(StrokeCap) override {}
  void blendMode(BlendMode) override {}
  void shader(rcp<RenderShader>) override { shaderWrites++; }
  void shaderTransform(const Mat2D& value) override { transform = value; }
  void invalidateStroke() override {}
};

class ShaderTransformFactory : public NoOpFactory
{
public:
  rcp<RenderPaint> makeRenderPaint() override
  {
    return make_rcp<ShaderTransformPaint>();
  }
};

// A 10x10 rectangle filled with a two stop sweep.
AngularGradient* makeSweep(Artboard& artboard, Fill** fillOut)
{
  auto* shape = new Shape();
  artboard.addObject(shape);

  auto* rect = new Rectangle();
  rect->width(10.0f);
  rect->height(10.0f);
  artboard.addObject(rect);
  rect->parentId(artboard.idOf(shape));

  auto* fill = new Fill();
  artboard.addObject(fill);
  fill->parentId(artboard.idOf(shape));

  auto* gradient = new AngularGradient();
  artboard.addObject(gradient);
  gradient->parentId(artboard.idOf(fill));

  for (float position : {0.0f, 1.0f})
  {
    auto* stop = new GradientStop();
    stop->colorValue(0xFFFF0000);
    stop->position(position);
    artboard.addObject(stop);
    stop->parentId(artboard.idOf(gradient));
  }

  *fillOut = fill;
  return gradient;
}
} // namespace

TEST_CASE("angular gradient rebuilds when its transform changes", "[gradient]")
{
  ShaderTransformFactory factory;
  Artboard artboard(&factory);
  artboard.addObject(&artboard);
  Fill* fill = nullptr;
  auto* gradient = makeSweep(artboard, &fill);
  REQUIRE(artboard.initialize() == StatusCode::Ok);
  artboard.advance(0.0f);

  auto* paint = static_cast<ShaderTransformPaint*>(fill->renderPaint());
  REQUIRE(paint != nullptr);
  REQUIRE(paint->shaderWrites > 0);
  CHECK(paint->transform == Mat2D());

  SECTION("translation")
  {
    gradient->transformX(25.0f);
    gradient->transformY(-5.0f);
    artboard.advance(0.0f);
    CHECK(paint->transform == Mat2D::fromTranslate(25.0f, -5.0f));
  }

  SECTION("rotation")
  {
    gradient->transformRotation(1.0f);
    artboard.advance(0.0f);
    CHECK(paint->transform == Mat2D::fromRotation(1.0f));
  }

  SECTION("scale")
  {
    gradient->transformScaleX(2.0f);
    gradient->transformScaleY(3.0f);
    artboard.advance(0.0f);
    CHECK(paint->transform == Mat2D::fromScale(2.0f, 3.0f));
  }
}
