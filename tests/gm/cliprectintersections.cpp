/*
 * Copyright 2023 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/math/math_types.hpp"
#include "rive/renderer.hpp"

using namespace rivegm;
using namespace rive;

// Ensures clipRect intersections are rendered properly, including the case
// where the clipRect goes negative and nothing should get drawn.
class CLipRectIntersectionsGM : public GM
{
public:
    CLipRectIntersectionsGM() : GM(150 + 120 * 3, 685, "cliprectintersections")
    {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* renderer) override
    {
        Paint cyan;
        cyan->color(0xff00ffff);

        Paint green;
        green->color(0xff00ff00);

        Paint red;
        red->color(0xffff0000);

        Paint stroke;
        stroke->style(RenderPaintStyle::stroke);
        stroke->color(0xb0ffffff);
        stroke->thickness(4);
        stroke->join(StrokeJoin::bevel);

        Path rect;
        rect->addRect(10, 10, 100, 100);

        Path fullscreen;
        fullscreen->addRect(-1e6, -1e6, 2e6, 2e6);

        float offsets[] = {0, 75, 150};

        renderer->save();
        for (size_t i = 0; i < 3; ++i)
        {
            renderer->save();
            renderer->clipPath(rect);
            renderer->drawPath(rect, cyan);
            renderer->restore();

            renderer->save();
            renderer->translate(offsets[i], 0);
            renderer->clipPath(rect);
            renderer->drawPath(fullscreen, cyan);
            renderer->restore();

            renderer->save();
            renderer->clipPath(rect);
            renderer->translate(offsets[i], 0);
            renderer->clipPath(rect);
            renderer->drawPath(fullscreen, i != 2 ? green : red);
            renderer->restore();

            renderer->save();
            renderer->drawPath(rect, stroke);
            renderer->translate(offsets[i], 0);
            renderer->drawPath(rect, stroke);
            renderer->restore();

            renderer->translate(0, 120);
        }
        renderer->restore();

        renderer->save();
        renderer->translate(150, 0);
        Paint majenta;
        majenta->color(0xffff00ff);
        for (size_t i = 0; i < 3; ++i)
        {
            renderer->save();
            renderer->clipPath(rect);
            renderer->drawPath(rect, majenta);
            renderer->restore();

            renderer->save();
            renderer->translate(0, offsets[i]);
            renderer->clipPath(rect);
            renderer->drawPath(fullscreen, majenta);
            renderer->restore();

            renderer->save();
            renderer->clipPath(rect);
            renderer->translate(0, offsets[i]);
            renderer->clipPath(rect);
            renderer->drawPath(fullscreen, i != 2 ? green : red);
            renderer->restore();

            renderer->save();
            renderer->drawPath(rect, stroke);
            renderer->translate(0, offsets[i]);
            renderer->drawPath(rect, stroke);
            renderer->restore();

            renderer->translate(120, 0);
        }
        renderer->restore();

        renderer->save();
        renderer->translate(0, 370);
        Paint yellow;
        majenta->color(0xffffff00);
        for (size_t i = 0; i < 3; ++i)
        {
            renderer->save();

            if (i == 1)
            {
                renderer->translate(0, 120);
            }
            else if (i == 2)
            {
                renderer->translate(120, 0);
            }

            renderer->save();
            renderer->clipPath(rect);
            renderer->drawPath(rect, majenta);
            renderer->restore();

            renderer->save();
            renderer->translate(offsets[i], offsets[i]);
            renderer->clipPath(rect);
            renderer->drawPath(fullscreen, majenta);
            renderer->restore();

            renderer->save();
            renderer->clipPath(rect);
            renderer->translate(offsets[i], offsets[i]);
            renderer->clipPath(rect);
            renderer->drawPath(fullscreen, i != 2 ? green : red);
            renderer->restore();

            renderer->save();
            renderer->drawPath(rect, stroke);
            renderer->translate(offsets[i], offsets[i]);
            renderer->drawPath(rect, stroke);
            renderer->restore();

            renderer->restore();
        }
        renderer->restore();
    }
};

GMREGISTER(return new CLipRectIntersectionsGM())
