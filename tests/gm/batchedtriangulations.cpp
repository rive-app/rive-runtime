/*
 * Copyright 2024 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "rive/math/math_types.hpp"

using namespace rivegm;
using namespace rive;

Path make_polygon(float cx, float cy, float r, int sides)
{
    float theta = 0;
    float dtheta = 2 * math::PI / sides;
    PathBuilder builder;
    builder.moveTo(cx + r, cy);
    for (int i = 1; i < sides; ++i)
    {
        theta += dtheta;
        builder.lineTo(cx + cosf(theta) * r, cy + sinf(theta) * r);
    }
    return builder.detach();
}

DEF_SIMPLE_GM(batchedtriangulations, 1600, 1600, renderer)
{
    Paint p;
    p->color(0xff00ffff);
    renderer->drawPath(PathBuilder::Circle(350, 350, 320), p);
    renderer->save();
    renderer->translate(1250, 350);
    renderer->rotate(math::PI / 5);
    renderer->scale(.8f, 1);
    p->color(0xffff00ff);
    renderer->drawPath(PathBuilder::Circle(0, 0, 320), p);
    renderer->restore();
    p->color(0xff80ff80);
    renderer->drawPath(make_polygon(1250, 1250, 320, 7), p);
    p->color(0xff8080ff);
    renderer->drawPath(make_polygon(350, 1250, 320, 5), p);
}
