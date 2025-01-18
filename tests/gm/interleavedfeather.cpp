/*
 * Copyright 2024 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "common/rand.hpp"
#include "rive/math/math_types.hpp"

using namespace rivegm;

namespace rive::gpu
{
// Verifies that draw call batching works correctly when drawing strokes and
// fills with and without feathers.
class InterleavedFeatherGM : public GM
{
public:
    InterleavedFeatherGM() : GM(1000, 1000, "interleavedfeather") {}

    ColorInt clearColor() const override { return 0; }

    void onDraw(Renderer* renderer) override
    {
        Rand rando;
        rando.seed(1);

        Path flower;
        flower->fillRule(FillRule::clockwise);
        constexpr static int NUM_PETALS = 7;
        constexpr static float R = 100;
        flower->moveTo(R, 0);
        for (int i = 1; i <= NUM_PETALS; ++i)
        {
            float c1 = 2 * M_PI * (i - 2 / 3.f) / NUM_PETALS;
            float c2 = 2 * M_PI * (i - 1 / 3.f) / NUM_PETALS;
            float theta = 2 * M_PI * i / NUM_PETALS;
            flower->cubicTo(cosf(c1) * R * 1.65,
                            sinf(c1) * R * 1.65,
                            cosf(c2) * R * 1.65,
                            sinf(c2) * R * 1.65,
                            cosf(theta) * R,
                            sinf(theta) * R);
        }
        path_addOval(flower,
                     AABB(-.6f * R, -.6f * R, .6f * R, .6f * R),
                     rivegm::PathDirection::ccw);
        flower->close();

        Paint paint;
        paint->blendMode(BlendMode::colorBurn);
        for (int i = 0; i < 300; ++i)
        {
            if (rando.boolean())
            {
                paint->feather(rando.f32(20));
            }
            else
            {
                paint->feather(expf(rando.f32(4)));
            }
            renderer->save();
            renderer->translate(rando.f32(0, 1000), rando.f32(0, 1000));
            float s = rando.f32(.25f, .5f);
            renderer->scale(s, s);
            renderer->rotate(rando.f32(math::PI * 2));

            paint->style(RenderPaintStyle::fill);
            paint->color(rando.u32() | 0x40808080);
            renderer->drawPath(flower.get(), paint.get());

            if (rando.boolean())
            {
                paint->style(RenderPaintStyle::stroke);
                paint->thickness(rando.f32(1, 8));
                paint->join(static_cast<StrokeJoin>(rando.u32(0, 2)));
                paint->color(rando.u32() & 0xff000000);
                renderer->drawPath(flower.get(), paint.get());
            }

            renderer->restore();
        }
    }
};
GMREGISTER(return new InterleavedFeatherGM)
} // namespace rive::gpu
