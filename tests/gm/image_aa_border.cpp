/*
 * Copyright 2023 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/math/math_types.hpp"

#include "assets/nomoon.png.hpp"

using namespace rive;
using namespace rivegm;

class ImageAABorderGM : public GM
{
public:
    ImageAABorderGM() : GM(500, 500) {}

    void onDraw(rive::Renderer* ren) override
    {
        auto img = LoadImage(assets::nomoon_png());
        if (!img)
        {
            return;
        }

        draw_image(ren, img.get(), {10, 10, 150, 120});

        {
            AutoRestore arr(ren, true);
            ren->translate(250, 250);
            ren->transform(Mat2D::fromRotation(-math::PI / 4.f));
            ren->scale(5.f, .25f);
            ren->transform(Mat2D::fromRotation(math::PI / 4.f));
            ren->translate(-40, -40);
            draw_image(ren, img.get(), {0, 0, 80, 80});
        }

        {
            AutoRestore arr(ren, true);
            ren->translate(360, 360);
            ren->scale(1, .8f);
            ren->transform(Mat2D::fromRotation(.9f));
            draw_image(ren, img.get(), {-80, -70, 80, 70});
        }

        draw_image(ren, img.get(), {160.5, 10, 161.5, 120});
        draw_image(ren, img.get(), {170.75, 10, 171.25, 120});
        draw_image(ren, img.get(), {180.875, 10, 181.125, 120});
        draw_image(ren, img.get(), {190.9375, 10, 191.0625, 120});

        draw_image(ren, img.get(), {10, 130.5, 150, 131.5});
        draw_image(ren, img.get(), {10, 140.75, 150, 141.25});
        draw_image(ren, img.get(), {10, 150.875, 150, 151.125});
        draw_image(ren, img.get(), {10, 160.9375, 150, 161.0625});
    }
};
GMREGISTER(image_aa_border, return new ImageAABorderGM)
