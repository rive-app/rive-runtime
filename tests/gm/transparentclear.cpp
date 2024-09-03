/*
 * Copyright 2023 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"

using namespace rivegm;
using namespace rive;

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(transparentclear, 0x8000ffff, 64, 64, renderer)
{
    Paint strokePaint;
    strokePaint->style(RenderPaintStyle::stroke);
    strokePaint->thickness(10);
    strokePaint->join(StrokeJoin::miter);
    strokePaint->color(0x80404000);
    renderer->drawPath(PathBuilder::Rect({0, 0, 64, 64}), strokePaint);
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(transparentclear_blendmode, 0x8000ffff, 64, 64, renderer)
{
    Paint strokePaint;
    strokePaint->style(RenderPaintStyle::stroke);
    strokePaint->thickness(10);
    strokePaint->join(StrokeJoin::miter);
    strokePaint->color(0x80404000);
    strokePaint->blendMode(BlendMode::colorBurn);
    renderer->drawPath(PathBuilder::Rect({0, 0, 64, 64}), strokePaint);
}
