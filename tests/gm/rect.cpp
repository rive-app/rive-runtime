/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"

using namespace rivegm;

class RectGM : public GM
{
public:
    RectGM() : GM(240, 240, "rect") {}

    void onDraw(rive::Renderer* ren) override
    {
        const rive::AABB rect = {0, 0, 120, 120};
        const rive::ColorInt colors[] = {
            0x80000000,
            0x80FF0000,
            0x8000FF00,
            0x800000FF,
        };

        Paint paint;
        ren->translate(10, 10);
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                AutoRestore ar(ren, true);

                ren->translate(x * 100, y * 100);
                paint->color(colors[y * 2 + x]);
                draw_rect(ren, rect, paint);
            }
        }
    }
};
GMREGISTER(return new RectGM)

class RectGradientGM : public GM
{
public:
    RectGradientGM() : GM(220, 220, "rect_grad") {}

    void onDraw(rive::Renderer* ren) override
    {
        const rive::AABB rect = {0, 0, 100, 100};
        const rive::ColorInt colors[] = {
            0xFFFF0000,
            0xFF00FF00,
            0xFF0000FF,
        };
        const rive::ColorInt grays[] = {
            0xFF000000,
            0xFFFFFFFF,
        };
        const float pos[] = {0, 0.5, 1};
        const float pos2[] = {0, 1};

        struct
        {
            rive::Vec2D a, b;
            int n;
            const rive::ColorInt* c;
            const float* p;
        } rec[] = {
            {{rect.minX, rect.minY}, {rect.maxX, rect.maxY}, 3, colors, pos},
            {{rect.maxX, rect.minY}, {rect.minX, rect.maxY}, 3, colors, pos},
            {{rect.maxX, rect.minY}, {rect.minX, rect.maxY}, 2, grays, pos2},
            {{rect.minX, rect.minY}, {rect.maxX, rect.maxY}, 2, grays, pos2},
        };

        Paint paint;
        ren->translate(10, 10);
        int i = 0;
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                AutoRestore ar(ren, true);

                ren->translate(x * 100, y * 100);
                auto shader = TestingWindow::Get()->factory()->makeLinearGradient(rec[i].a.x,
                                                                                  rec[i].a.y,
                                                                                  rec[i].b.x,
                                                                                  rec[i].b.y,
                                                                                  rec[i].c,
                                                                                  rec[i].p,
                                                                                  rec[i].n);
                paint->shader(shader);
                draw_rect(ren, rect, paint);
                i += 1;
            }
        }
    }
};
GMREGISTER(return new RectGradientGM)
