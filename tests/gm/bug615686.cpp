/*
 * Copyright 2016 Google Inc.
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

DEF_SIMPLE_GM(bug615686, 250, 250, renderer)
{
    Paint p;
    p->style(RenderPaintStyle::stroke);
    p->thickness(20);
    renderer->drawPath(PathBuilder().moveTo(0, 0).cubicTo(200, 200, 0, 200, 200, 0).detach(), p);
}
