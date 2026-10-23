/*
 * Copyright 2024 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"

#include "common/testing_window.hpp"
#include "rive/renderer/render_context.hpp"

#include "assets/nomoon.png.hpp"

#include <iterator>

using namespace rive;
using namespace rivegm;

// Grid of squares filled and stroked with linear and radial gradients, each
// under a translated, scaled or rotated shader transform.
DEF_SIMPLE_GM_WITH_CLEAR_COLOR(
    gradienttransform,
    0xff000000,
    1600,
    800,
    renderer)
{
  gpu::RenderContext* renderContext = TestingWindow::Get()->renderContext();
  gpu::RenderContext::FrameDescriptor frameDescriptor;
  if (renderContext != nullptr)
  {
    frameDescriptor = renderContext->frameDescriptor();
    frameDescriptor.loadAction = gpu::LoadAction::preserveRenderTarget;
  }

  // This also tests addRenderPath's transform
  Path doubleSquare;
  doubleSquare->addRect(0.0f, 0.0f, 2.0f, 2.0f);

  Path rect;
  rect->addRenderPath(doubleSquare.get(), Mat2D::fromScale(0.5f, 0.5f));

  ColorInt colors[] = {0xffff0000, 0xff00ff00};
  float stops[] = {0.0f, 1.0f};

  Paint horizPaint;
  horizPaint->shader(
      TestingWindow::Get()
          ->factory()
          ->makeLinearGradient(0.0f, 0.0f, 1.0f, 0.0f, colors, stops, 2));

  Paint vertPaint;
  vertPaint->shader(
      TestingWindow::Get()
          ->factory()
          ->makeLinearGradient(0.0f, 0.0f, 0.0f, 1.0f, colors, stops, 2));

  Paint radialPaint;
  radialPaint->shader(
      TestingWindow::Get()
          ->factory()
          ->makeRadialGradient(0.5f, 0.5f, 0.5f, colors, stops, 2));

  Paint angularPaint;
  angularPaint->shader(
      TestingWindow::Get()->factory()->makeAngularGradient(colors, stops, 2));

  // The rect is a unit square that the renderer scales up, so stroke
  // thickness is in unit-square space.
  constexpr float strokeThickness = 0.2f;
  horizPaint->thickness(strokeThickness);
  vertPaint->thickness(strokeThickness);
  radialPaint->thickness(strokeThickness);
  angularPaint->thickness(strokeThickness);

  std::tuple<const Paint&, Mat2D> iterations[] = {
      {horizPaint, Mat2D::fromTranslate(0.25f, 0.0f)},
      {horizPaint, Mat2D::fromTranslate(-0.25f, 0.0f)},
      {horizPaint, Mat2D::fromScale(1.5f, 1.0f)},
      {horizPaint, Mat2D::fromScale(0.5f, 1.0f)},
      {horizPaint, Mat2D::fromRotation(math::PI * 0.125f)},
      {horizPaint, Mat2D::fromRotation(-math::PI * 0.125f)},
      {vertPaint, Mat2D::fromTranslate(0.0f, 0.25f)},
      {vertPaint, Mat2D::fromTranslate(0.0f, -0.25f)},
      {vertPaint, Mat2D::fromScale(1.0f, 1.5f)},
      {vertPaint, Mat2D::fromScale(1.0f, 0.5f)},
      {vertPaint, Mat2D::fromRotation(math::PI * 0.125f)},
      {vertPaint, Mat2D::fromRotation(-math::PI * 0.125f)},
      {radialPaint, Mat2D::fromTranslate(0.25f, 0.0f)},
      {radialPaint, Mat2D::fromTranslate(-0.25f, 0.0f)},
      {radialPaint, Mat2D::fromTranslate(0.0f, 0.25f)},
      {radialPaint, Mat2D::fromTranslate(0.0f, -0.25f)},
      {radialPaint, Mat2D::fromScale(1.5f, 1.0f)},
      {radialPaint, Mat2D::fromScale(0.5f, 1.0f)},
      {radialPaint, Mat2D::fromScale(1.0f, 1.5f)},
      {radialPaint, Mat2D::fromScale(1.0f, 0.5f)},
      {radialPaint, Mat2D::fromRotation(math::PI * 0.125f)},
      {radialPaint, Mat2D::fromRotation(-math::PI * 0.125f)},
      {angularPaint,
       Mat2D::fromTranslate(0.5f, 0.5f) * Mat2D::fromTranslate(0.25f, 0.25f)},
      {angularPaint,
       Mat2D::fromTranslate(0.5f, 0.5f) * Mat2D::fromScale(1.0f, 1.5f)},
      {angularPaint,
       Mat2D::fromTranslate(0.5f, 0.5f) * Mat2D::fromScale(1.0f, 0.5f)},
      {angularPaint,
       Mat2D::fromTranslate(0.5f, 0.5f) *
           Mat2D::fromRotation(-math::PI * 0.125f)},
      {angularPaint,
       Mat2D::fromTranslate(0.5f, 0.5f) *
           Mat2D::fromRotation(-math::PI * 0.125f)},
  };

  // Each iteration is drawn twice: filled, then stroked.
  int count = sizeof(iterations) / sizeof(iterations[0]) * 2;

  // Grid dimensions that keep the cells roughly square.
  double aspectRatio = double(width()) / double(height());
  int xCount = int(std::ceil(std::sqrt(double(count) * aspectRatio)));
  int yCount = (count + xCount - 1) / xCount;

  float rectSpaceWidth = float(width()) / float(xCount);
  float rectSpaceHeight = float(height()) / float(yCount);
  float rectWidth = rectSpaceWidth * 0.75f;
  float rectHeight = rectSpaceHeight * 0.75f;

  for (int i = 0; i < count; i++)
  {
    int x = i % xCount;
    int y = i / xCount;

    TestingWindow::Get()->flushPLSContext();

    if (renderContext != nullptr)
    {
      renderContext->beginFrame(frameDescriptor);
    }

    renderer->save();
    renderer->translate(
        (float(x) + 0.5f) * rectSpaceWidth - rectWidth * 0.5f,
        (float(y) + 0.5f) * rectSpaceHeight - rectHeight * 0.5f);
    renderer->scale(rectWidth, rectHeight);

    auto& [paint, transform] = iterations[i / 2];
    paint->style(
        (i % 2) == 0 ? RenderPaintStyle::fill : RenderPaintStyle::stroke);
    paint->shaderTransform(transform);

    renderer->drawPath(rect, paint);
    renderer->restore();
  }
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(gradienttypes, 0xff404040, 2048, 1536, renderer)
{
  auto factory = TestingWindow::Get()->factory();

  ColorInt colors2[] = {
      0xff881177,
      0xff22ccbb,
  };
  float stops2[] = {0.0f, 1.0f};
  static_assert(std::size(colors2) == std::size(stops2));

  ColorInt colors5[] = {
      0xff99dd55,
      0xff00bbcc,
      0xff881177,
      0xffcc6666,
      0xffeedd00,
  };
  float stops5[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
  static_assert(std::size(colors5) == std::size(stops5));

  rcp<RenderShader> gradients[] = {
      factory->makeLinearGradient(
          -100.0f,
          -100.0f,
          100.0f,
          100.0f,
          colors2,
          stops2,
          std::size(colors2)),
      factory->makeRadialGradient(
          0.0f,
          0.0,
          100.0f,
          colors2,
          stops2,
          std::size(colors2)),
      factory->makeAngularGradient(colors2, stops2, std::size(colors2)),
      factory->makeLinearGradient(
          -100.0f,
          -100.0f,
          100.0f,
          100.0f,
          colors5,
          stops5,
          std::size(colors5)),
      factory->makeRadialGradient(
          0.0f,
          0.0,
          100.0f,
          colors5,
          stops5,
          std::size(colors5)),
      factory->makeAngularGradient(colors5, stops5, std::size(colors5)),
  };

  auto box = AABB{-110.0f, -110.0f, 110.0f, 110.0f};
  auto path = PathBuilder::RRect(box, float(40.0f), float(40.0f));
  path->fillRule(FillRule::clockwise);

  auto img = LoadImage(assets::nomoon_png());

  for (auto drawTypeIndex = 0; drawTypeIndex < 8; drawTypeIndex++)
  {
    auto useImage = (drawTypeIndex & 1) != 0;
    bool isStroke = (drawTypeIndex & 2) != 0;
    bool isFeather = (drawTypeIndex & 4) != 0;

    for (auto gradientIndex = 0; gradientIndex < std::size(gradients);
         gradientIndex++)
    {
      renderer->save();
      renderer->transform(Mat2D::fromTranslate(
          float(drawTypeIndex) * 256.0f + 128.0f,
          float(gradientIndex) * 256.0f + 128.0f));

      Paint paint;
      paint->shader(gradients[gradientIndex]);
      if (isStroke)
      {
        paint->stroke({.thickness = 10.0f});
      }

      if (isFeather)
      {
        paint->feather(20.0f);
      }

      if (useImage)
      {
        paint->modulatedImage(
            img.get(),
            ImageSampler{},
            Mat2D::fromTranslate(-110.0f, -110.0f) *
                Mat2D::fromScale(220.0f, 220.0f));
      }

      renderer->drawPath(path, paint);

      renderer->restore();
    }
  }
}