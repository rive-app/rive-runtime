/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/math/math_types.hpp"

using namespace rivegm;

static void add_star(Path& path, int count, float dir)
{
    if (count & 1)
    {
        path_add_star(path, count, 0, dir);
    }
    else
    {
        count >>= 1;
        path_add_star(path, count, 0, dir);
        path_add_star(path,
                      count,
                      rive::math::PI,
                      1); // always wind one on the 2nd contour
    }
}

class PolyGM : public GM
{
    rive::FillRule m_FillRule;

public:
    PolyGM(rive::FillRule fr) : GM(480, 480), m_FillRule(fr) {}

    void onDraw(rive::Renderer* ren) override
    {
        auto draw =
            [this,
             ren](float tx, float ty, rive::ColorInt c, int pts, float dir) {
                Path path;
                path->fillRule(m_FillRule);
                add_star(path, pts, dir);

                AutoRestore ar(ren, true);
                ren->translate(tx, ty);
                ren->scale(100, 100);
                ren->drawPath(path, Paint(c));
            };

        draw(120, 120, 0xAAFF0000, 5, 1);
        draw(360, 120, 0xAA0000FF, 6, 1);
        draw(360, 360, 0xAA00FF00, 5, -1);
        draw(120, 360, 0xAA000000, 6, -1);
    }
};

// Expect all to be filled but the black-6-pointer
GMREGISTER(poly_nonZero, return new PolyGM(rive::FillRule::nonZero))

// Expect all to have a hole
GMREGISTER(poly_evenOdd, return new PolyGM(rive::FillRule::evenOdd))

// Expect all to be filled but the black-6-pointer, and the black-6-pointer will
// also be missing half the triangle tips.
GMREGISTER(poly_clockwise, return new PolyGM(rive::FillRule::clockwise))
