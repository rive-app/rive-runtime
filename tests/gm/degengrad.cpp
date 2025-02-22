/*
 * Copyright 2023 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"

using namespace rivegm;
using namespace rive;

// Make sure gradients don't go NaN-tastic when they have co-located
// (degenerate) stops, and that they render as specified when stops are
// co-located.
class DegenGradGM : public GM
{
public:
    DegenGradGM() : GM(200 * 4 + 50, 200 * 3 + 50) {}

    void onDraw(rive::Renderer* renderer) override
    {
        Factory* factory = TestingWindow::Get()->factory();

        auto rect = factory->makeEmptyRenderPath();
        rect->addRect(0, 0, 150.f, 150.f);

        auto paint = factory->makeRenderPaint();

        renderer->translate(50, 50);

        ColorInt colors2[] = {0xff0000ff, 0xff00ff00};
        // Stuff a red stop in the middle and ensure no red shows up in the
        // drawing.
        ColorInt colors3[] = {0xff0000ff, 0xffff0000, 0xff00ff00};

        for (float stop : {.0f, .5f, 1.f})
        {
            float stops[] = {stop, stop, stop};

            renderer->save();

            paint->shader(
                factory->makeLinearGradient(0, 0, 150, 150, colors2, stops, 2));
            renderer->drawPath(rect.get(), paint.get());

            renderer->translate(200, 0);
            paint->shader(
                factory->makeRadialGradient(75, 75, 75, colors2, stops, 2));
            renderer->drawPath(rect.get(), paint.get());

            renderer->translate(200, 0);
            paint->shader(
                factory->makeLinearGradient(0, 0, 150, 150, colors3, stops, 3));
            renderer->drawPath(rect.get(), paint.get());

            renderer->translate(200, 0);
            paint->shader(
                factory->makeRadialGradient(75, 75, 75, colors3, stops, 3));
            renderer->drawPath(rect.get(), paint.get());

            renderer->restore();
            renderer->translate(0, 200);
        }
    }
};

GMREGISTER(degengrad, return new DegenGradGM())
