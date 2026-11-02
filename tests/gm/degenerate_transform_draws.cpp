/*
 * Copyright 2026 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/color.hpp"
#include <cmath>

using namespace rivegm;
using namespace rive;

// Draws under transforms that collapse an axis (determinant 0) cover no area,
// so they must not change a pixel. degenerate_transform_draws draws fills,
// strokes, feathers, gradients and clips that way over a background, at
// fractional offsets so the renderer doesn't cull them for empty bounds;
// degenerate_transform_draws_ref draws only the background. The two must
// match exactly.
namespace
{
constexpr int kCell = 100;
constexpr int kCols = 9;
constexpr int kRows = 6;

void drawBackground(Renderer* renderer)
{
  Paint background(0xffe8e4dc);
  renderer->drawPath(
      PathBuilder::Rect({0, 0, kCell * kCols, kCell * kRows}),
      background);
  // Cell borders, so the image isn't one flat color.
  Paint line(0xffc8c0b4);
  for (int x = 0; x <= kCols; ++x)
  {
    renderer->drawPath(
        PathBuilder::Rect(
            {x * kCell - 0.5f, 0, x * kCell + 0.5f, kCell * kRows}),
        line);
  }
  for (int y = 0; y <= kRows; ++y)
  {
    renderer->drawPath(
        PathBuilder::Rect(
            {0, y * kCell - 0.5f, kCell * kCols, y * kCell + 0.5f}),
        line);
  }
}

Mat2D collapse(int row)
{
  switch (row)
  {
    case 0:
      return Mat2D(1, 0, 0, 0, 0, 0); // scale y 0
    case 1:
      return Mat2D(0, 0, 0, 1, 0, 0); // scale x 0
    case 2:
      return Mat2D(0, 0, 0, 0, 0, 0); // scale 0
    case 3:
    {
      // Rotated, then collapsed: a diagonal line.
      const float c = cosf(0.6f), s = sinf(0.6f);
      return Mat2D(c, s, -s, c, 0, 0) * Mat2D(1, 0, 0, 0, 0, 0);
    }
    case 4:
      return Mat2D(1, 1, 1, 1, 0, 0); // skew to a line
    default:
      return Mat2D(0.6f, 0.3f, 1.2f, 0.6f, 0, 0); // general rank 1
  }
}

void drawShape(Renderer* renderer, int col)
{
  // Centered on the origin, 70 wide.
  auto rect = []() { return PathBuilder::Rect({-35, -25, 35, 25}); };
  auto oval = []() {
    Path path;
    path_addOval(path, {-35, -30, 35, 30});
    return path;
  };
  switch (col)
  {
    case 0:
    {
      Paint paint(0xff202060);
      renderer->drawPath(rect(), paint);
      break;
    }
    case 1:
    {
      Path path = oval();
      path_addOval(path, {-15, -15, 15, 15});
      path->fillRule(FillRule::evenOdd);
      Paint paint(0xff602020);
      renderer->drawPath(path, paint);
      break;
    }
    case 2:
    {
      Path path = oval();
      path->fillRule(FillRule::clockwise);
      Paint paint(0xff206020);
      renderer->drawPath(path, paint);
      break;
    }
    case 3:
    {
      Paint paint(0xff000000);
      paint->style(RenderPaintStyle::stroke);
      paint->thickness(12);
      paint->join(StrokeJoin::miter);
      renderer->drawPath(rect(), paint);
      break;
    }
    case 4:
    {
      Paint paint(0xff402060);
      paint->style(RenderPaintStyle::stroke);
      paint->thickness(6);
      paint->cap(StrokeCap::round);
      renderer->drawPath(oval(), paint);
      break;
    }
    case 5:
    {
      // Feathered fills only draw clockwise.
      Path path = oval();
      path->fillRule(FillRule::clockwise);
      Paint paint(0xff006060);
      paint->feather(16);
      renderer->drawPath(path, paint);
      break;
    }
    case 6:
    {
      Paint paint(0xff600060);
      paint->style(RenderPaintStyle::stroke);
      paint->thickness(8);
      paint->feather(10);
      renderer->drawPath(rect(), paint);
      break;
    }
    case 7:
    {
      Paint paint;
      ColorInt colors[] = {0xff0000ff, 0xffff0000};
      float stops[] = {0, 1};
      paint->shader(TestingWindow::Get()
                        ->factory()
                        ->makeLinearGradient(-35, 0, 35, 0, colors, stops, 2));
      renderer->drawPath(oval(), paint);
      break;
    }
    case 8:
    {
      // A clip and a draw under it, both collapsed.
      renderer->clipPath(oval());
      Paint paint(0xff000000);
      renderer->drawPath(PathBuilder::Rect({-50, -50, 50, 50}), paint);
      break;
    }
  }
}
} // namespace

class DegenerateTransformDrawsGM : public GM
{
public:
  DegenerateTransformDrawsGM(bool drawCollapsed)
      : GM(kCell * kCols, kCell * kRows)
      , m_drawCollapsed(drawCollapsed)
  {}

  void onDraw(Renderer* renderer) override
  {
    drawBackground(renderer);
    if (!m_drawCollapsed)
    {
      return;
    }
    for (int row = 0; row < kRows; ++row)
    {
      for (int col = 0; col < kCols; ++col)
      {
        renderer->save();
        // Off the pixel grid, so collapsed bounds still span pixels.
        renderer->translate(col * kCell + 50.37f, row * kCell + 50.61f);
        renderer->transform(collapse(row));
        drawShape(renderer, col);
        renderer->restore();
      }
    }
    // Strokes and feathers whose collapsed outline lands on a cell border.
    renderer->save();
    renderer->translate(450, 300);
    renderer->transform(Mat2D(1, 0, 0, 0, 0, 0));
    Paint thick(0xff000000);
    thick->style(RenderPaintStyle::stroke);
    thick->thickness(30);
    renderer->drawPath(PathBuilder::Rect({-400, -100, 400, 100}), thick);
    renderer->restore();
  }

private:
  const bool m_drawCollapsed;
};

GMREGISTER(
    degenerate_transform_draws,
    return new DegenerateTransformDrawsGM(true))
GMREGISTER(
    degenerate_transform_draws_ref,
    return new DegenerateTransformDrawsGM(false))
