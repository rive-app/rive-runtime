/*
 * Copyright 2024 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/math/math_types.hpp"

using namespace rivegm;

namespace rive::gpu
{
// Verifies that fill rules still get drawn properly when the content
// interleaves them.
class InterleavedFillRuleGM : public GM
{
public:
    InterleavedFillRuleGM() : GM(1700, 1700, "interleavedfillrule") {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(Renderer* renderer) override
    {
        renderer->scale(2, 2);
        std::vector<Path> stars;
        for (uint32_t x = 0; x < 7; ++x)
        {
            Path& path = stars.emplace_back();
            uint32_t n = 5 + x;
            if (n >= 8)
            {
                ++n;
            }
            if (n % 3 == 0 && !((n / 3) & 1))
            {
                int ntri = n / 3;
                float dtheta = 2 * rive::math::PI / ntri;
                for (int i = 0; i < ntri; ++i)
                {
                    path_add_star(path, 3, dtheta * i, (i & 1) ? -1 : 1);
                }
            }
            else if (n & 1)
            {
                path_add_star(path, n, 0, 1);
            }
            else
            {
                path_add_star(path, (n / 2) | 1, 0, 1);
                path_add_star(path, (n / 2) | 1, rive::math::PI, -1);
            }
        }
        for (uint32_t y = 0; y < 7; ++y)
        {
            renderer->save();
            for (uint32_t x = 0; x < 7; ++x)
            {
                renderer->save();
                bool flipMatrix = false;
                if (y == 0)
                {
                    stars[x]->fillRule(FillRule::nonZero);
                }
                else if (y == 1)
                {
                    stars[x]->fillRule(FillRule::evenOdd);
                    flipMatrix = true;
                }
                else if (y == 2)
                {
                    stars[x]->fillRule(FillRule::clockwise);
                }
                else if (y == 3)
                {
                    stars[x]->fillRule(FillRule::clockwise);
                    flipMatrix = true;
                }
                else
                {
                    stars[x]->fillRule(static_cast<FillRule>((x + y) / 2 % 3));
                    flipMatrix = (x ^ y) & 1;
                }
                renderer->translate(60, 60);
                renderer->scale(50, 50);
                if (flipMatrix)
                {
                    if (x & 1)
                        renderer->scale(-1, 1);
                    else
                        renderer->scale(1, -1);
                }
                renderer->drawPath(
                    stars[x].get(),
                    Paint(((y * x + 123458383u) * 285018463u) | 0xff808080));
                renderer->restore();
                renderer->translate(120, 0);
            }
            renderer->restore();
            renderer->translate(0, 120);
        }
    }
};
GMREGISTER(return new InterleavedFillRuleGM)
} // namespace rive::gpu
