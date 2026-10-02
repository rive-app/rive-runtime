/*
 * Copyright 2026 Rive
 */

// Inside/center/outside strokes on the shapes the circle-only stroke_position
// GMs don't reach. Each cell draws the stroke, then the path's fill region in
// translucent purple, then the path itself as a thin red line.
//
// Pinned behavior, all intended:
//  - Inside/outside close open paths, so the closing segment is stroked too.
//  - Joins only exist on the outer side of a turn: inside strokes show them
//    at concave corners, outside strokes at convex ones.
//  - Which side is "inside" follows the stroke path's own fill rule, so holes
//    and contours wound in opposite directions resolve as that rule says.

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"

using namespace rivegm;
using namespace rive;

namespace
{
constexpr float kCell = 160.0f;
constexpr float kThickness = 18.0f;

Path openZigzag()
{
    return PathBuilder()
        .moveTo(24, 50)
        .lineTo(60, 120)
        .lineTo(96, 50)
        .lineTo(136, 120)
        .detach();
}

// Four convex corners and one concave one, at the notch.
Path notchedSquare()
{
    return PathBuilder()
        .moveTo(30, 30)
        .lineTo(130, 30)
        .lineTo(130, 130)
        .lineTo(80, 80)
        .lineTo(30, 130)
        .close()
        .detach();
}

// Both squares wound the same way, so only even-odd sees a hole.
Path squareWithHole(FillRule fillRule)
{
    Path path = PathBuilder()
                    .moveTo(30, 30)
                    .lineTo(130, 30)
                    .lineTo(130, 130)
                    .lineTo(30, 130)
                    .close()
                    .moveTo(60, 60)
                    .lineTo(100, 60)
                    .lineTo(100, 100)
                    .lineTo(60, 100)
                    .close()
                    .detach();
    path->fillRule(fillRule);
    return path;
}

// A clockwise square beside a counter-clockwise one, as two pen-drawn paths
// in one shape can be.
Path oppositeSquares()
{
    return PathBuilder()
        .moveTo(20, 40)
        .lineTo(90, 40)
        .lineTo(90, 110)
        .lineTo(20, 110)
        .close()
        .moveTo(110, 70)
        .lineTo(110, 120)
        .lineTo(140, 120)
        .lineTo(140, 70)
        .close()
        .detach();
}

void drawBackground(Renderer* renderer, float width, float height)
{
    Paint bg;
    bg->color(0xff000000);
    renderer->drawPath(PathBuilder::Rect({0, 0, width, height}), bg);
}

// [strokePath] is what gets stroked; [fillPath] only shows the region the
// stroke is measured against, and is usually the same path.
void drawCell(Renderer* renderer,
              const Path& strokePath,
              const Path& fillPath,
              StrokeParams params,
              float feather)
{
    Paint stroke;
    stroke->color(0xFFEFEFEF);
    stroke->stroke(params);
    stroke->feather(feather);
    renderer->drawPath(strokePath, stroke);

    Paint fill;
    fill->color(0x5f881177);
    renderer->drawPath(fillPath, fill);

    Paint line;
    line->color(0xFFFF3030);
    line->stroke({.thickness = 1.5f});
    renderer->drawPath(strokePath, line);
}

// Rows: shape x join. Columns: inside, center, outside.
void drawShapesByJoin(Renderer* renderer, float feather)
{
    drawBackground(renderer, 3 * kCell, 9 * kCell);

    Path shapes[] = {openZigzag(),
                     notchedSquare(),
                     squareWithHole(FillRule::evenOdd)};
    StrokeJoin joins[] = {StrokeJoin::miter,
                          StrokeJoin::round,
                          StrokeJoin::bevel};
    for (int s = 0; s < 3; ++s)
    {
        for (int j = 0; j < 3; ++j)
        {
            for (int p = 0; p < 3; ++p)
            {
                renderer->save();
                renderer->translate(p * kCell, (s * 3 + j) * kCell);
                drawCell(renderer,
                         shapes[s],
                         shapes[s],
                         {.thickness = kThickness,
                          .join = joins[j],
                          .position = StrokePosition(p)},
                         feather);
                renderer->restore();
            }
        }
    }
}
} // namespace

DEF_SIMPLE_GM(stroke_position_shapes, 480, 1440, renderer)
{
    drawShapesByJoin(renderer, 0.0f);
}

// Feathered inside/outside strokes take a different branch in the renderer:
// clip by the path, then draw the thickened stroke.
DEF_SIMPLE_GM(stroke_position_shapes_feathered, 480, 1440, renderer)
{
    drawShapesByJoin(renderer, 6.0f);
}

// What the editor sends: stroke paths are always non-zero, whatever the Fill's
// rule. Row 1 is a same-direction hole that an even-odd Fill shows, but the
// non-zero stroke path counts as inside. Row 2 is two opposite-wound squares.
// Columns: inside, center, outside.
DEF_SIMPLE_GM(stroke_position_winding, 480, 320, renderer)
{
    drawBackground(renderer, 3 * kCell, 2 * kCell);

    Path hole = squareWithHole(FillRule::nonZero);
    Path holeFill = squareWithHole(FillRule::evenOdd);
    Path pair = oppositeSquares();
    for (int p = 0; p < 3; ++p)
    {
        StrokeParams params = {.thickness = 12.0f,
                               .position = StrokePosition(p)};
        renderer->save();
        renderer->translate(p * kCell, 0);
        drawCell(renderer, hole, holeFill, params, 0.0f);
        renderer->translate(0, kCell);
        drawCell(renderer, pair, pair, params, 0.0f);
        renderer->restore();
    }
}
