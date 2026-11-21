/*
 * Copyright 2026 Rive
 */

// Octagons built from 8 separate triangles that share edges. With analytic AA,
// each triangle's edge is only partly covered, so the background bleeds
// through the seams unless the AA reaches outside the edge.
//
// Columns: inkbleed 0, .125 px, .25 px, .5 px. Rows: paint alpha 1, .89, .64.
//
// Each triangle's coverage on the shared edge is c = .5 + b (b is the
// inkbleed), and the two blend with srcOver, so the seam's center matches the
// interior when (1 - alpha*c)^2 = 1 - alpha, i.e., alpha = 2b / (.5 + b)^2.
// That gives each row its matching column: .5 px, .25 px, and .125 px.
//
// Even the matching column keeps a faint dark fringe beside the center, where
// one triangle is fully covered and the other's AA ramp still adds alpha:
// (1 - alpha) * (1 - alpha*c_other) < 1 - alpha. No alpha/inkbleed pair
// flattens the whole seam under srcOver except opaque at .5 px; that would
// need the edges' coverage combined before blending.

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "rive/math/math_types.hpp"

#ifdef WITH_RIVE_TEXT
#include "assets/roboto_flex.ttf.hpp"
#include "common/testing_window.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text.hpp"
#endif

using namespace rivegm;
using namespace rive;

constexpr static int Cell = 200;
constexpr static float Radius = 80;
// Margins for the row and column labels.
constexpr static int LabelWidth = 40;
constexpr static int LabelHeight = 30;

#ifdef WITH_RIVE_TEXT
static void drawLabel(
    Renderer* renderer,
    rcp<Font> font,
    const char* label,
    float x,
    float y,
    float rotation = 0)
{
  auto black = TestingWindow::Get()->factory()->makeRenderPaint();
  black->color(0xff000000);
  RawText text(TestingWindow::Get()->factory());
  text.maxWidth(Cell);
  text.sizing(TextSizing::fixed);
  text.append(label, black, font, 18.0f);
  renderer->save();
  renderer->translate(x, y);
  renderer->rotate(rotation);
  text.render(renderer);
  renderer->restore();
}
#endif

static void drawOctagon(Renderer* renderer, ColorInt color, uint8_t inkbleed)
{
  // Rotated and off-center so no edge lines up with the pixel grid.
  constexpr float Cx = Cell * .5f + .3f;
  constexpr float Cy = Cell * .5f + .3f;
  constexpr float Rotation = math::PI / 17;
  Paint paint;
  paint->color(color);
  paint->inkbleed(inkbleed);
  for (int i = 0; i < 8; ++i)
  {
    float theta0 = Rotation + i * (2 * math::PI / 8);
    float theta1 = Rotation + (i + 1) * (2 * math::PI / 8);
    PathBuilder builder;
    builder.moveTo(Cx, Cy);
    builder.lineTo(Cx + cosf(theta0) * Radius, Cy + sinf(theta0) * Radius);
    builder.lineTo(Cx + cosf(theta1) * Radius, Cy + sinf(theta1) * Radius);
    builder.close();
    renderer->drawPath(builder.detach(), paint);
  }
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(
    inkbleed,
    0xffffffff,
    LabelWidth + Cell * 4,
    LabelHeight + Cell * 3,
    renderer)
{
#ifdef WITH_RIVE_TEXT
  auto font = HBFont::Decode(assets::roboto_flex_ttf());
  const char* colLabels[] = {
      "inkbleed 0 px",
      "inkbleed .125 px",
      "inkbleed .25 px",
      "inkbleed .5 px",
  };
  for (int i = 0; i < 4; ++i)
  {
    drawLabel(renderer, font, colLabels[i], LabelWidth + i * Cell + 40, 4);
  }
  const char* rowLabels[] = {"alpha 1", "alpha .89", "alpha .64"};
  for (int i = 0; i < 3; ++i)
  {
    drawLabel(
        renderer,
        font,
        rowLabels[i],
        8,
        LabelHeight + i * Cell + Cell * .5f + 35,
        // Reads bottom to top.
        -math::PI / 2);
  }
#endif

  renderer->translate(LabelWidth, LabelHeight);
  // 0, .125, .25, and .5 px. (0xff => 1/2px.)
  constexpr uint8_t Inkbleeds[] = {0, 0x40, 0x80, 0xff};
  // Alpha 1, .89 (8/9), and .64 (16/25).
  for (ColorInt color : {0xff2060c0u, 0xe32060c0u, 0xa32060c0u})
  {
    renderer->save();
    for (uint8_t inkbleed : Inkbleeds)
    {
      drawOctagon(renderer, color, inkbleed);
      renderer->translate(Cell, 0);
    }
    renderer->restore();
    renderer->translate(0, Cell);
  }
}
