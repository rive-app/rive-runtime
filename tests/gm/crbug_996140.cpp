/*
 * Copyright 2019 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"

using namespace rivegm;
using namespace rive;

/*
 * Canvas example. Expected large blue stroked circle, white middle, small red
 * circle. GPU-accelerated canvas produces large blue stroked circle, white
 * middle, NO red circle. 1:  var c = document.getElementById("myCanvas"); 2:
 * var ctx = c.getContext("2d"); 3:  ctx.beginPath(); 4:  ctx.scale(203.20,
 * 203.20); 5:  ctx.translate(-14.55, -711.51); 6:  ctx.fillStyle = "red"; 7:
 * ctx.strokeStyle = "blue"; 8:  //ctx.lineWidth = 1/203.20; 9:  ctx.arc(19.221,
 * 720-6.76,0.0295275590551181,0,2*Math.PI); 10: ctx.stroke(); 11: ctx.fill();
 * 12: ctx.closePath();
 */
DEF_SIMPLE_GM(crbug_996140, 300, 300, renderer)
{
    // Specific parameters taken from the canvas minimum working example
    float cx = 19.221f;
    float cy = 720 - 6.76f;
    float radius = 0.0295275590551181f;

    float s = 203.20f;
    float tx = -14.55f;
    float ty = -711.51f;

    // 0: The test canvas was 1920x574 and the circle was located in the bottom
    // left, but that's not necessary to reproduce the problem, so translate to
    // make a smaller GM.
    renderer->translate(-800, -200);

    // 3: ctx.beginPath();

    // 4: ctx.scale(203.20, 203.20);
    renderer->scale(s, s);
    // 5: ctx.translate(-14.55, -711.51);
    renderer->translate(tx, ty);

    // 6: ctx.fillStyle = "red";
    Paint fill;
    fill->color(0xff0000ff);
    fill->style(RenderPaintStyle::fill);

    // 7: ctx.strokeStyle = "blue";
    Paint stroke;
    stroke->color(0xffff0000);
    stroke->thickness(1.f);
    stroke->style(RenderPaintStyle::stroke);

    // 9: ctx.arc(19.221, 720-6.76,0.0295275590551181,0,2*Math.PI);
    // This matches how Canvas prepares an arc(x, y, radius, 0, 2pi) call
    auto boundingBox = AABB(cx - radius, cy - radius, cx + radius, cy + radius);

    Path path;
    path_addOval(path, boundingBox);

    // 12: ctx.closePath();
    // path.close();

    // 10: ctx.stroke(); (NOT NEEDED TO REPRODUCE FAILING RED CIRCLE)
    renderer->drawPath(path, stroke);
    // 11: ctx.fill()
    renderer->drawPath(path, fill);
}
