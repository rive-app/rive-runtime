/*
 * Copyright 2025 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"

using namespace rivegm;
using namespace rive;

constexpr int SIZE = 1600;

// Tests that interior triangulations with negative coverage render correctly
// with clockwise fill, both as paths and as clips.
static void draw_test(Renderer* renderer, bool asClip)
{
    PathBuilder checkerboard;
    checkerboard.fillRule(FillRule::clockwise);
    constexpr static int GRID_COUNT = 50;
    constexpr static float CELL_SIZE = (float)SIZE / GRID_COUNT;
    for (int y = 0; y < GRID_COUNT; y += 1)
    {
        checkerboard.addRect(AABB(0, y * CELL_SIZE, SIZE, (y + 1) * CELL_SIZE),
                             (y & 1) ? rivegm::PathDirection::cw
                                     : rivegm::PathDirection::ccw);
    }
    for (int x = 0; x < GRID_COUNT; x += 1)
    {
        checkerboard.addRect(AABB(x * CELL_SIZE, 0, (x + 1) * CELL_SIZE, SIZE),
                             (x & 1) ? rivegm::PathDirection::cw
                                     : rivegm::PathDirection::ccw);
    }
    renderer->clipPath(checkerboard.detach());

    Path path;
    path->fillRule(FillRule::clockwise);
    // Add a negative rectangle.
    path->addRect(SIZE, 0, -SIZE, SIZE);

    // The first path will be completely erased by the negative rectangle. It
    // will only show if we draw it over itself twice.
    for (float x : {SIZE / 2.f, .0f, SIZE / 2.f})
    {
        path->moveTo(x + 50, SIZE / 2.5f);
        path->cubicTo(x + 50,
                      0,
                      x + SIZE / 2.f - 50,
                      0,
                      x + SIZE / 2.f - 50,
                      SIZE / 2.5f);
        path->cubicTo(x + SIZE / 2.f - 50,
                      SIZE,
                      x + 50,
                      SIZE,
                      x + 50,
                      SIZE / 2.5f);
    }

    if (asClip)
    {
        Paint red;
        red->color(0xffff0000);
        renderer->clipPath(path.get());
        renderer->drawPath(PathBuilder::Rect({0, 0, SIZE, SIZE}).get(),
                           red.get());
    }
    else
    {
        Paint magenta;
        magenta->color(0xffff00ff);
        renderer->drawPath(path.get(), magenta.get());
    }
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(negative_interior_triangles,
                               0xff00ffff,
                               SIZE,
                               SIZE,
                               renderer)
{
    draw_test(renderer, /*asClip=*/false);
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(negative_interior_triangles_as_clip,
                               0xff00ffff,
                               SIZE,
                               SIZE,
                               renderer)
{
    draw_test(renderer, /*asClip=*/true);
}
