/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/math/math_types.hpp"

using namespace rivegm;

static void make_star(Path& path, int count, float anglePhase, float dir)
{
    assert(count & 1);
    float da = 2 * rive::math::PI * (count >> 1) / count;
    float angle = anglePhase;
    for (int i = 0; i < count; ++i)
    {
        rive::Vec2D p = {cosf(angle), sinf(angle)};
        i == 0 ? path->move(p) : path->line(p);
        angle += da * dir;
    }
}

static void add_star(Path& path, int count, float dir)
{
    if (count & 1)
    {
        make_star(path, count, 0, dir);
    }
    else
    {
        count >>= 1;
        make_star(path, count, 0, dir);
        make_star(path,
                  count,
                  rive::math::PI,
                  1); // always wind one on the 2nd contour
    }
}

static std::string fillrule_to_name(rive::FillRule fr)
{
    return std::string("poly_") +
           (fr == rive::FillRule::nonZero ? "nonZero" : "evenOdd");
}

class PolyGM : public GM
{
    rive::FillRule m_FillRule;

public:
    PolyGM(rive::FillRule fr) :
        GM(480, 480, fillrule_to_name(fr).c_str()), m_FillRule(fr)
    {}

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
GMREGISTER(return new PolyGM(rive::FillRule::nonZero))

// Expect all to have a hole
GMREGISTER(return new PolyGM(rive::FillRule::evenOdd))
