/*
 * Copyright 2021 Google LLC.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "rive/math/math_types.hpp"

using namespace rivegm;
using namespace rive;

class BatchedConvexPathsGM : public GM
{
public:
    BatchedConvexPathsGM() : GM(512, 512) {}

private:
    ColorInt clearColor() const override { return 0xFF000000; }

    void onDraw(Renderer* canvas) override
    {
        for (uint32_t i = 0; i < 10; ++i)
        {
            canvas->save();

            int numPoints = (i + 3) * 3;
            Path path;
            path->moveTo(1, 0);
            for (float j = 1; j < numPoints; j += 3)
            {
                constexpr float k2PI = math::PI * 2;
                path->cubicTo(
                    cosf(j / numPoints * k2PI),
                    sinf(j / numPoints * k2PI),
                    cosf((j + 1) / numPoints * k2PI),
                    sinf((j + 1) / numPoints * k2PI),
                    j + 2 == numPoints ? 1 : cosf((j + 2) / numPoints * k2PI),
                    j + 2 == numPoints ? 0 : sinf((j + 2) / numPoints * k2PI));
            }
            float scale = 256 - i * 24;
            canvas->translate(scale + (256 - scale) * .33f,
                              scale + (256 - scale) * .33f);
            canvas->scale(scale, scale);

            Paint paint;
            paint->color(colorModulateOpacity(((i + 123458383u) * 285018463u) |
                                                  0xff808080,
                                              0.3f));

            canvas->drawPath(path, paint);

            canvas->restore();
        }
    }
};

GMREGISTER(batchedconvexpaths, return new BatchedConvexPathsGM;)
