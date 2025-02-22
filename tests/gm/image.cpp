/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"

#include "assets/batdude.png.hpp"
#include "assets/nomoon.png.hpp"

using namespace rivegm;

class ImageGM : public GM
{
public:
    ImageGM() : GM(530, 310) {}

    void onDraw(rive::Renderer* ren) override
    {
        rive::Span<uint8_t> images[] = {
            assets::batdude_png(),
            assets::nomoon_png(),
        };

        rive::AABB r = {0, 0, 250, 250};

        ren->save();
        ren->translate(10, 10);
        for (auto image : images)
        {
            auto img = LoadImage(image);
            if (!img)
            {
                ren->restore();
                Paint p;
                p->color(0xFFFF0000);
                draw_rect(ren, {0, 0, 530, 310}, p);
                return;
            }
            draw_image(ren, img.get(), r);

            ren->translate(r.width() + 10, 0);
        }
        ren->restore();
    }
};
GMREGISTER(image, return new ImageGM)
