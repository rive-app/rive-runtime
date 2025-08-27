/*
 * Copyright 2011 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/math/math_types.hpp"

using namespace rivegm;
using namespace rive;

namespace
{
struct PathDY
{
    Path path;
    float dy;
};
} // namespace

typedef PathDY (*MakePathProc)();

static PathDY make_triangle()
{
    constexpr int gCoord[] = {10, 20, 15, 5, 30, 30};
    return {PathBuilder()
                .moveTo(static_cast<float>(gCoord[0] + 10),
                        static_cast<float>(gCoord[1]))
                .lineTo(static_cast<float>(gCoord[2] + 10),
                        static_cast<float>(gCoord[3]))
                .lineTo(static_cast<float>(gCoord[4] + 10),
                        static_cast<float>(gCoord[5]))
                .close()
                .detach(),
            30};
}

static PathDY make_rect()
{
    AABB r = {static_cast<float>(10 + 10),
              static_cast<float>(10),
              static_cast<float>(30 + 10),
              static_cast<float>(30)};
    return {PathBuilder().addRect(r).detach(), 30};
}

static PathDY make_oval()
{
    AABB r = {static_cast<float>(10 + 20),
              static_cast<float>(10),
              static_cast<float>(30 + 20),
              static_cast<float>(30)};
    return {PathBuilder().addOval(r).detach(), 30};
}

static PathDY make_sawtooth(int teeth)
{
    float x = static_cast<float>(20);
    float y = static_cast<float>(20);
    const float x0 = x;
    const float dx = static_cast<float>(5);
    const float dy = static_cast<float>(10);

    PathBuilder builder;
    builder.moveTo(x, y);
    for (int i = 0; i < teeth; i++)
    {
        x += dx;
        builder.lineTo(x, y - dy);
        x += dx;
        builder.lineTo(x, y + dy);
    }
    builder.lineTo(x, y + (2 * dy));
    builder.lineTo(x0, y + (2 * dy));
    builder.close();

    return {builder.detach(), 30};
}

static PathDY make_sawtooth_3() { return make_sawtooth(3); }
static PathDY make_sawtooth_32() { return make_sawtooth(32); }

static PathDY make_house()
{
    PathBuilder builder;
    builder
        .addPolygon({{20 + 21, 23},
                     {20 + 21, 11.534f},
                     {20 + 22.327f, 12.741f},
                     {20 + 23.673f, 11.261f},
                     {20 + 12, 0.648f},
                     {20 + 8, 4.285f},
                     {20 + 8, 2},
                     {20 + 4, 2},
                     {20 + 4, 7.921f},
                     {20 + 0.327f, 11.26f},
                     {20 + 1.673f, 12.74f},
                     {20 + 3, 11.534f},
                     {20 + 3, 23},
                     {20 + 11, 23},
                     {20 + 11, 18},
                     {20 + 13, 18},
                     {20 + 13, 23},
                     {20 + 21, 23}},
                    true)
        .polylineTo({{20 + 9, 16},
                     {20 + 9, 21},
                     {20 + 5, 21},
                     {20 + 5, 9.715f},
                     {20 + 12, 3.351f},
                     {20 + 19, 9.715f},
                     {20 + 19, 21},
                     {20 + 15, 21},
                     {20 + 15, 16},
                     {20 + 9, 16}})
        .close();
    return {builder.detach(), 30};
}

static PathDY make_star(int n)
{
    const float c = static_cast<float>(45);
    const float r = static_cast<float>(20);

    float rad = -rive::math::PI / 2;
    const float drad = (n >> 1) * rive::math::PI * 2 / n;

    PathBuilder builder;
    builder.moveTo(c, c - r);
    for (int i = 1; i < n; i++)
    {
        rad += drad;
        builder.lineTo(c + cosf(rad) * r, c + sinf(rad) * r);
    }
    builder.close();

    return {builder.detach(), r * 2 * 6 / 5};
}

static PathDY make_star_5() { return make_star(5); }
static PathDY make_star_13() { return make_star(13); }

// We don't expect any output from this path.
static PathDY make_line()
{
    return {PathBuilder()
                .moveTo(30, 30)
                .lineTo(120, 40)
                .close()
                .moveTo(150, 30)
                .lineTo(150, 30)
                .lineTo(300, 40)
                .close()
                .detach(),
            40};
}

static Path make_info()
{
    PathBuilder path;
    path.moveTo(24, 4);
    path.cubicTo(12.94999980926514f, 4, 4, 12.94999980926514f, 4, 24);
    path.cubicTo(4, 35.04999923706055f, 12.94999980926514f, 44, 24, 44);
    path.cubicTo(35.04999923706055f, 44, 44, 35.04999923706055f, 44, 24);
    path.cubicTo(44, 12.95000076293945f, 35.04999923706055f, 4, 24, 4);
    path.close();
    path.moveTo(26, 34);
    path.lineTo(22, 34);
    path.lineTo(22, 22);
    path.lineTo(26, 22);
    path.lineTo(26, 34);
    path.close();
    path.moveTo(26, 18);
    path.lineTo(22, 18);
    path.lineTo(22, 14);
    path.lineTo(26, 14);
    path.lineTo(26, 18);
    path.close();
    return path.detach();
}

static Path make_accessibility()
{
    PathBuilder path;
    path.moveTo(12, 2);
    path.cubicTo(13.10000038146973f, 2, 14, 2.900000095367432f, 14, 4);
    path.cubicTo(14, 5.099999904632568f, 13.10000038146973f, 6, 12, 6);
    path.cubicTo(10.89999961853027f, 6, 10, 5.099999904632568f, 10, 4);
    path.cubicTo(10, 2.900000095367432f, 10.89999961853027f, 2, 12, 2);
    path.close();
    path.moveTo(21, 9);
    path.lineTo(15, 9);
    path.lineTo(15, 22);
    path.lineTo(13, 22);
    path.lineTo(13, 16);
    path.lineTo(11, 16);
    path.lineTo(11, 22);
    path.lineTo(9, 22);
    path.lineTo(9, 9);
    path.lineTo(3, 9);
    path.lineTo(3, 7);
    path.lineTo(21, 7);
    path.lineTo(21, 9);
    path.close();
    return path.detach();
}

// test case for http://crbug.com/695196
static Path make_visualizer()
{
    PathBuilder path;
    path.moveTo(1.9520f, 2.0000f);
    path.quadTo(1.5573f, 1.9992f, 1.2782f, 2.2782f);
    path.quadTo(0.9992f, 2.5573f, 1.0000f, 2.9520f);
    path.lineTo(1.0000f, 5.4300f);
    path.lineTo(17.0000f, 5.4300f);
    path.lineTo(17.0000f, 2.9520f);
    path.quadTo(17.0008f, 2.5573f, 16.7218f, 2.2782f);
    path.quadTo(16.4427f, 1.9992f, 16.0480f, 2.0000f);
    path.lineTo(1.9520f, 2.0000f);
    path.close();
    path.moveTo(2.7140f, 3.1430f);
    path.quadTo(3.0547f, 3.1287f, 3.2292f, 3.4216f);
    path.quadTo(3.4038f, 3.7145f, 3.2292f, 4.0074f);
    path.quadTo(3.0547f, 4.3003f, 2.7140f, 4.2860f);
    path.quadTo(2.1659f, 4.2631f, 2.1659f, 3.7145f);
    path.quadTo(2.1659f, 3.1659f, 2.7140f, 3.1430f);
    path.lineTo(2.7140f, 3.1430f);
    path.close();
    path.moveTo(5.0000f, 3.1430f);
    path.quadTo(5.3407f, 3.1287f, 5.5152f, 3.4216f);
    path.quadTo(5.6898f, 3.7145f, 5.5152f, 4.0074f);
    path.quadTo(5.3407f, 4.3003f, 5.0000f, 4.2860f);
    path.quadTo(4.4519f, 4.2631f, 4.4519f, 3.7145f);
    path.quadTo(4.4519f, 3.1659f, 5.0000f, 3.1430f);
    path.lineTo(5.0000f, 3.1430f);
    path.close();
    path.moveTo(7.2860f, 3.1430f);
    path.quadTo(7.6267f, 3.1287f, 7.8012f, 3.4216f);
    path.quadTo(7.9758f, 3.7145f, 7.8012f, 4.0074f);
    path.quadTo(7.6267f, 4.3003f, 7.2860f, 4.2860f);
    path.quadTo(6.7379f, 4.2631f, 6.7379f, 3.7145f);
    path.quadTo(6.7379f, 3.1659f, 7.2860f, 3.1430f);
    path.close();
    path.moveTo(1.0000f, 6.1900f);
    path.lineTo(1.0000f, 14.3810f);
    path.quadTo(0.9992f, 14.7757f, 1.2782f, 15.0548f);
    path.quadTo(1.5573f, 15.3338f, 1.9520f, 15.3330f);
    path.lineTo(16.0480f, 15.3330f);
    path.quadTo(16.4427f, 15.3338f, 16.7218f, 15.0548f);
    path.quadTo(17.0008f, 14.7757f, 17.0000f, 14.3810f);
    path.lineTo(17.0000f, 6.1910f);
    path.lineTo(1.0000f, 6.1910f);
    path.lineTo(1.0000f, 6.1900f);
    path.close();
    return path.detach();
}

constexpr MakePathProc gProcs[] = {
    make_triangle,
    make_rect,
    make_oval,
    make_sawtooth_32,
    make_star_5,
    make_star_13,
    make_line,
    make_house,
    make_sawtooth_3,
};

#define N std::size(gProcs)

class PathFillGM : public GM
{
public:
    PathFillGM() : GM(365, 425) {}

private:
    Path fPath[N];
    float fDY[N];
    Path fInfoPath;
    Path fAccessibilityPath;
    Path fVisualizerPath;

protected:
    void onOnceBeforeDraw() override
    {
        for (size_t i = 0; i < N; i++)
        {
            auto [path, dy] = gProcs[i]();
            fPath[i] = std::move(path);
            fDY[i] = dy;
        }

        fInfoPath = make_info();
        fAccessibilityPath = make_accessibility();
        fVisualizerPath = make_visualizer();
    }

    void onDraw(Renderer* canvas) override
    {
        Paint paint;

        for (size_t i = 0; i < N; i++)
        {
            canvas->drawPath(fPath[i], paint);
            canvas->translate(static_cast<float>(0), fDY[i]);
        }

        canvas->save();
        canvas->scale(0.300000011920929f, 0.300000011920929f);
        canvas->translate(50, 50);
        canvas->drawPath(fInfoPath, paint);
        canvas->restore();

        canvas->scale(2, 2);
        canvas->translate(5, 15);
        canvas->drawPath(fAccessibilityPath, paint);

        canvas->scale(0.5f, 0.5f);
        canvas->translate(5, 50);
        canvas->drawPath(fVisualizerPath, paint);
    }
};

DEF_SIMPLE_GM(rotatedcubicpath, 100, 100, canvas)
{
    Paint p;
    p->style(RenderPaintStyle::fill);

    canvas->translate(20, 40);
    Path path;
    path->moveTo(48, -23);
    path->cubicTo(48, -29.5, 6, -30, 6, -30);
    path->cubicTo(6, -30, 2, 0, 2, 0);
    path->cubicTo(2, 0, 44, -21.5, 48, -23);
    path->close();

    p->color(0xff0000ff);
    canvas->drawPath(path, p);

    // Rotated path, which is not antialiased on GPU
    p->color(0xffff0000);
    canvas->rotate(90);
    canvas->drawPath(path, p);
}

///////////////////////////////////////////////////////////////////////////////

GMREGISTER(pathfill, return new PathFillGM;)

DEF_SIMPLE_GM(bug7792, 800, 800, canvas)
{
    // from skbug.com/7792 bug description
    Paint p;
    Path path;
    path->moveTo(10, 10);
    path->moveTo(75, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(75, 150);
    canvas->drawPath(path, p);
    // from skbug.com/7792#c3
    canvas->translate(200, 0);
    path = Path();
    path->moveTo(75, 50);
    path->moveTo(100, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(75, 150);
    path->lineTo(75, 50);
    path->close();
    canvas->drawPath(path, p);
    // from skbug.com/7792#c9
    canvas->translate(200, 0);
    path = Path();
    path->moveTo(10, 10);
    path->moveTo(75, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(75, 150);
    path->close();
    canvas->drawPath(path, p);
    // from skbug.com/7792#c11
    canvas->translate(-200 * 2, 200);
    path = Path();
    path->moveTo(75, 150);
    path->lineTo(75, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(75, 150);
    path->moveTo(75, 150);
    canvas->drawPath(path, p);
    // from skbug.com/7792#c14
    canvas->translate(200, 0);
    path = Path();
    path->moveTo(250, 75);
    path->moveTo(250, 75);
    path->moveTo(250, 75);
    path->moveTo(100, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(75, 150);
    path->lineTo(75, 75);
    path->close();
    path->lineTo(0, 0);
    path->close();
    canvas->drawPath(path, p);
    // from skbug.com/7792#c15
    canvas->translate(200, 0);
    path = Path();
    path->moveTo(75, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(75, 150);
    path->moveTo(250, 75);
    canvas->drawPath(path, p);
    // from skbug.com/7792#c17
    canvas->translate(-200 * 2, 200);
    path = Path();
    path->moveTo(75, 10);
    path->moveTo(75, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(75, 150);
    path->lineTo(75, 10);
    path->close();
    canvas->drawPath(path, p);
    // from skbug.com/7792#c19
    canvas->translate(200, 0);
    path = Path();
    path->moveTo(75, 75);
    path->lineTo(75, 75);
    path->lineTo(75, 75);
    path->lineTo(75, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(75, 150);
    path->close();
    path->moveTo(10, 10);
    path->lineTo(30, 10);
    path->lineTo(10, 30);
    canvas->drawPath(path, p);
    // from skbug.com/7792#c23
    canvas->translate(200, 0);
    path = Path();
    path->moveTo(75, 75);
    path->lineTo(75, 75);
    path->moveTo(75, 75);
    path->lineTo(75, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(75, 150);
    path->close();
    canvas->drawPath(path, p);
    // from skbug.com/7792#c29
    canvas->translate(-200 * 2, 200);
    path = Path();
    path->moveTo(75, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(75, 150);
    path->lineTo(75, 250);
    path->moveTo(75, 75);
    path->close();
    canvas->drawPath(path, p);
    // from skbug.com/7792#c31
    canvas->translate(200, 0);
    path = Path();
    path->moveTo(75, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(75, 150);
    path->lineTo(75, 10);
    path->moveTo(75, 75);
    path->close();
    canvas->drawPath(path, p);
    // from skbug.com/7792#c36
    canvas->translate(200, 0);
    path = Path();
    path->moveTo(75, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(10, 150);
    path->moveTo(75, 75);
    path->lineTo(75, 75);
    canvas->drawPath(path, p);
    // from skbug.com/7792#c39
    canvas->translate(200, -200 * 3);
    path = Path();
    path->moveTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(75, 150);
    path->lineTo(75, 100);
    canvas->drawPath(path, p);
    // from zero_length_paths_aa
    canvas->translate(0, 200);
    path = Path();
    path->moveTo(150, 100);
    path->lineTo(150, 100);
    path->lineTo(150, 150);
    path->lineTo(75, 150);
    path->lineTo(75, 100);
    path->lineTo(75, 75);
    path->lineTo(150, 75);
    path->close();
    canvas->drawPath(path, p);
    // from skbug.com/7792#c41
    canvas->translate(0, 200);
    path = Path();
    path->moveTo(75, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(140, 150);
    path->lineTo(140, 75);
    path->moveTo(75, 75);
    path->close();
    canvas->drawPath(path, p);
    // from skbug.com/7792#c53
    canvas->translate(0, 200);
    path = Path();
    path->moveTo(75, 75);
    path->lineTo(150, 75);
    path->lineTo(150, 150);
    path->lineTo(140, 150);
    path->lineTo(140, 75);
    path->moveTo(75, 75);
    path->close();
    canvas->drawPath(path, p);
}

DEF_SIMPLE_GM(path_stroke_clip_crbug1070835, 30, 25, canvas)
{
    Paint p;
    p->color(0xffff0000);
    p->style(RenderPaintStyle::stroke);
    p->thickness(2);

    canvas->scale(4.16666651f / 2, 4.16666651f / 2);

    PathBuilder path;

    Vec2D pts[] = {
        {11, 12},
        {11, 18.0751324f},
        {6.07513189f, 23},
        {-4.80825292E-7f, 23},
        {-6.07513332f, 23},
        {-11, 18.0751324f},
        {-11, 11.999999f},
        {-10.999999f, 5.92486763f},
        {-6.07513189f, 1},
        {1.31173692E-7f, 1},
        {6.07513141f, 1},
        {10.9999981f, 5.92486572f},
        {11, 11.9999971f},
    };
    path.moveTo(pts[0].x, pts[0].y)
        .cubicTo(pts[1].x, pts[1].y, pts[2].x, pts[2].y, pts[3].x, pts[3].y)
        .cubicTo(pts[4].x, pts[4].y, pts[5].x, pts[5].y, pts[6].x, pts[6].y)
        .cubicTo(pts[7].x, pts[7].y, pts[8].x, pts[8].y, pts[9].x, pts[9].y)
        .cubicTo(pts[10].x,
                 pts[10].y,
                 pts[11].x,
                 pts[11].y,
                 pts[12].x,
                 pts[12].y);

    canvas->drawPath(path.detach(), p);
}

DEF_SIMPLE_GM(path_skbug_11859, 512, 512, canvas)
{
    Paint paint;
    paint->color(0xffff0000);

    Path path;
    path->moveTo(258, -2);
    path->lineTo(258, 258);
    path->lineTo(237, 258);
    path->lineTo(240, -2);
    path->lineTo(258, -2);
    path->moveTo(-2, -2);
    path->lineTo(240, -2);
    path->lineTo(238, 131);
    path->lineTo(-2, 131);
    path->lineTo(-2, -2);

    canvas->scale(2, 2);
    canvas->drawPath(path, paint);
}

DEF_SIMPLE_GM(path_skbug_11886, 128, 256, canvas)
{
    Vec2D m = {0.f, 770.f};
    Path path;
    path->moveTo(m.x, m.y);
    path->cubicTo(m.x + 0.f,
                  m.y + 1.f,
                  m.x + 20.f,
                  m.y - 750.f,
                  m.x + 83.f,
                  m.y + -746.f);
    Paint paint;
    canvas->drawPath(path, paint);
}
