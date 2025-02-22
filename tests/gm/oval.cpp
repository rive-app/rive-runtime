/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"

using namespace rivegm;

class OvalGM : public GM
{
public:
    OvalGM() : GM(410, 410) {}

    void onDraw(rive::Renderer* ren) override
    {
        Paint paint;

        paint->color(0xFFFF0000);
        draw_oval(ren, {10, 10, 100, 50}, paint.get());
        paint->color(0xFF00FF00);
        draw_oval(ren, {10, 60, 50, 200}, paint.get());

        rive::AABB r = {70, 70, 200, 200};
        paint->color(0xFF0000FF);

        Path path;
        path_addOval(path, r, PathDirection::cw);
        path_addOval(path, r.inset(20, 20), PathDirection::ccw);
        ren->drawPath(path, paint);

        path = Path();
        r = {210, 10, 210 + 100, 10 + 100};
        path_addOval(path, r);
        path_addOval(path, r.offset(80, 30));
        path_addOval(path, r.offset(30, 80));
        paint->color(0xFFCC8844);
        ren->drawPath(path, paint);

        ren->translate(-95, 205);
        path->fillRule(rive::FillRule::evenOdd);
        paint->color(0xFF4488CC);
        ren->drawPath(path, paint);
    }
};
GMREGISTER(oval, return new OvalGM)
