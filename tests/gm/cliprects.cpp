/*
 * Copyright 2023 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/math/math_types.hpp"
#include "rive/renderer.hpp"
#include "skia/include/utils/SkRandom.h"

using namespace rivegm;
using namespace rive;

constexpr static int kNumRows = 3;

// Ensures that rectangular clips (which are handled as clipRects) get rendered correctly.
class ClipRectsGM : public GM
{
public:
    ClipRectsGM() : GM(200 * 3, 200 * kNumRows, "cliprects") {}

    void onDraw(rive::Renderer* renderer) override
    {
        SkRandom rand;
        Factory* factory = TestingWindow::Get()->factory();

        Paint outerPaint;
        outerPaint->color(0xe0102040);

        Paint innerPaint;

        for (size_t i = 0; i < kNumRows; ++i)
        {
            AABB outerRect = AABB::fromLTWH(25.f, 25.f + 200.f * i, 150.f, 150.f);
            AABB innerRect = outerRect.inset(5, 5);

            if (i == kNumRows - 1)
            {
                renderer->translate(0, outerRect.center().y * 2);
                renderer->scale(1, -1);
            }

            auto outerPath = factory->makeEmptyRenderPath();
            outerPath->addRect(outerRect.left(),
                               outerRect.top(),
                               outerRect.width(),
                               outerRect.height());

            auto innerPath = factory->makeEmptyRenderPath();
            innerPath->addRect(innerRect.left(),
                               innerRect.top(),
                               innerRect.width(),
                               innerRect.height());

            renderer->save();

            renderer->save();
            renderer->drawPath(outerPath.get(), outerPaint);
            innerPaint->color(rand.nextU() | 0xff808080);
            renderer->clipPath(innerPath.get());
            renderer->drawPath(outerPath.get(), innerPaint);
            renderer->restore();

            renderer->translate(200, 0);
            renderer->save();
            renderer->translate(outerRect.center().x, outerRect.center().y);
            renderer->rotate(rive::math::PI / 8);
            renderer->translate(-outerRect.center().x, -outerRect.center().y);
            renderer->drawPath(outerPath.get(), outerPaint);
            innerPaint->color(rand.nextU() | 0xff808080);
            renderer->clipPath(innerPath.get());
            renderer->drawPath(outerPath.get(), innerPaint);
            renderer->restore();

            renderer->translate(200, 0);
            renderer->save();
            renderer->translate(outerRect.center().x, outerRect.center().y);
            renderer->transform(Mat2D{.9f, .1f, -.3f, .7f, 0, 0});
            renderer->translate(-outerRect.center().x, -outerRect.center().y);
            renderer->drawPath(outerPath.get(), outerPaint);
            innerPaint->color(rand.nextU() | 0xff808080);
            renderer->clipPath(innerPath.get());
            renderer->drawPath(outerPath.get(), innerPaint);
            renderer->restore();

            renderer->restore();
        }
    }
};

GMREGISTER(return new ClipRectsGM())
