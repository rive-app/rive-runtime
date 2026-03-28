/*
 * Copyright 2026 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/shapes/paint/color.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/renderer.hpp"

using namespace rivegm;
using namespace rive;

// Causes gamma correction to get used by un
class GammaCorrectionGM : public GM
{
public:
    GammaCorrectionGM() : GM(500, 500) {}

private:
    void onDraw(Renderer* canvas) override
    {

        {
            Paint paint(0xFFB000B0);
            Path path;
            path->addRect(50, 50, 400, 400);
            canvas->drawPath(path, paint);
        }

        canvas->clipPath(PathBuilder::Rect({240, 240, 260, 260}));

        {
            Paint paint(0xFFF0B000);
            Path path;
            path->addRect(200, 200, 100, 100);
            canvas->drawPath(path, paint);
        }
    }
};

GMREGISTER(gamma_correction_clip, return new GammaCorrectionGM)