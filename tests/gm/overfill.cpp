/*
 * Copyright 2025 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"

using namespace rivegm;
using namespace rive;

// Check that clockwise paths render & blend correctly with winding numbers much
// larger than 1.
// It's especially important to check that clockwise render modes with
// transparency don't corrupt the paint after coverage is fully saturated.
static void draw_overfill_paths(ColorInt color0,
                                BlendMode mode0,
                                ColorInt color1,
                                BlendMode mode1,
                                ColorInt color2,
                                BlendMode mode2,
                                Renderer* renderer)
{
    PathBuilder pathBuilder;
    pathBuilder.fillRule(FillRule::clockwise);
    for (float r : {10, 50, 40, 80, 90, 70, 60, 20, 50, 30})
    {
        pathBuilder.addCircle(100, 100, r);
    }
    auto path = pathBuilder.detach();

    Paint paint;
    paint->color(color0);
    paint->blendMode(mode0);
    renderer->translate(-30, -30);
    renderer->drawPath(path.get(), paint.get());

    paint->color(color1);
    paint->blendMode(mode1);
    renderer->translate(60, 0);
    renderer->drawPath(path.get(), paint.get());

    paint->color(color2);
    paint->blendMode(mode2);
    renderer->translate(0, 60);
    renderer->drawPath(path.get(), paint.get());
}

DEF_SIMPLE_GM(overfill_opaque, 200, 200, renderer)
{
    draw_overfill_paths(0xff00ff00,
                        BlendMode::srcOver,
                        0xff0000ff,
                        BlendMode::srcOver,
                        0xffff0000,
                        BlendMode::srcOver,
                        renderer);
}

DEF_SIMPLE_GM(overfill_transparent, 200, 200, renderer)
{
    draw_overfill_paths(0x6000ff00,
                        BlendMode::srcOver,
                        0x600000ff,
                        BlendMode::srcOver,
                        0x60ff0000,
                        BlendMode::srcOver,
                        renderer);
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(overfill_blendmodes,
                               0x20202020,
                               200,
                               200,
                               renderer)
{
    draw_overfill_paths(0xc040f010,
                        BlendMode::colorDodge,
                        0xc01040f0,
                        BlendMode::hardLight,
                        0xc0f01040,
                        BlendMode::luminosity,
                        renderer);
}
