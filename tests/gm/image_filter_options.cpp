/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include "assets/low_res_image.h"

using namespace rivegm;

class ImageFilterOptionsGM : public GM
{
public:
    ImageFilterOptionsGM() : GM(1020, 1020) {}

    void onDraw(rive::Renderer* ren) override
    {
        auto img =
            LoadImage(rive::make_span(test_image_png, test_image_png_len));

        if (!img)
        {
            ren->restore();
            Paint p;
            p->color(0xFFFF0000);
            draw_rect(ren, {0, 0, 1020, 1020}, p);
            return;
        }

        rive::AABB r = {0, 0, 500, 500};
        ren->save();
        ren->translate(10, 10);
        draw_image(ren,
                   img.get(),
                   {rive::ImageWrap::clamp,
                    rive::ImageWrap::clamp,
                    rive::ImageFilter::bilinear},
                   r);
        ren->restore();
        ren->save();
        ren->translate(510, 10);
        draw_image(ren,
                   img.get(),
                   {rive::ImageWrap::clamp,
                    rive::ImageWrap::clamp,
                    rive::ImageFilter::nearest},
                   r);
        ren->restore();
        ren->save();
        ren->translate(10, 510);
        draw_image(ren,
                   img.get(),
                   {rive::ImageWrap::clamp,
                    rive::ImageWrap::clamp,
                    rive::ImageFilter::bilinear},
                   r);
        ren->restore();
        ren->save();
        ren->translate(510, 510);
        draw_image(ren,
                   img.get(),
                   {rive::ImageWrap::clamp,
                    rive::ImageWrap::clamp,
                    rive::ImageFilter::nearest},
                   r);
        ren->restore();
    }
};
GMREGISTER(image_filter_options, return new ImageFilterOptionsGM)
