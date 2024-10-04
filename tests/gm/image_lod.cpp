/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"

#include "assets/batdude.png.hpp"

using namespace rivegm;

class ImageLODGM : public GM
{
public:
    ImageLODGM() : GM(978, 919, "image_lod") {}

    void onDraw(rive::Renderer* renderer) override
    {
        rive::AABB r = {0, 0, 442, 412};
        renderer->save();
        auto img = LoadImage(assets::batdude_png());
        assert(img != nullptr);

        renderer->translate(10, 10);

        constexpr static int numLods = 9;
        renderer->save();
        for (int i = 0; i < numLods; ++i)
        {
            draw_image(renderer, img.get(), r);
            renderer->translate(0, 412 + 10 * (1 << i));
            renderer->scale(1, .5);
        }
        renderer->restore();

        renderer->save();
        for (int i = 0; i < numLods; ++i)
        {
            draw_image(renderer, img.get(), r);
            renderer->translate(442 + 10 * (1 << i), 0);
            renderer->scale(.5, 1);
        }
        renderer->restore();

        renderer->save();
        for (int i = 0; i < numLods; ++i)
        {
            draw_image(renderer, img.get(), r);
            renderer->translate(442 + 10 * (1 << i), 412 + 10 * (1 << i));
            renderer->scale(.5, .5);
        }
        renderer->restore();

        renderer->restore();
    }
};
GMREGISTER(return new ImageLODGM)
