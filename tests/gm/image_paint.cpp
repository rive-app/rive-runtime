/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"

#include "assets/batdude.png.hpp"
#include "assets/nomoon.png.hpp"

using namespace rivegm;

class ImagePaintGM : public GM
{
public:
    ImagePaintGM() : GM(512, 512) {}

    void onDraw(rive::Renderer* ren) override
    {
        rive::Span<uint8_t> images[] = {
            assets::batdude_png(),
            assets::nomoon_png(),
        };

        rive::AABB r = {0, 0, 250, 250};

        {
            Paint p;
            p->color(0xFF2f2f2f);
            draw_rect(ren, {0, 0, 512, 512}, p);
        }

        rive::Factory* factory = TestingWindow::Get()->factory();

        ren->save();
        ren->translate(256, 256);

        auto img0 = LoadImage(images[0]);
        auto img1 = LoadImage(images[1]);
        if (img0 == nullptr || img1 == nullptr)
        {
            ren->restore();
            Paint p;
            p->color(0xFFFF0000);
            draw_rect(ren, {0, 0, 530, 310}, p);
            return;
        }

        {
            Paint p;
            p->color(0xFFB299FF);
            rive::ColorInt gradientColors[] = {
                0xFFFF9B9Bu,
                0xFFC5FF8Cu,
                0xFF70A6FFu,
            };

            float gradientStops[] = {0.0f, 0.5f, 1.0f};
            static_assert(std::size(gradientColors) ==
                          std::size(gradientStops));
            p->modulatedImage(
                img0.get(),
                {
                    .wrapX = rive::ImageWrap::mirror,
                    .wrapY = rive::ImageWrap::repeat,
                    .filter = rive::ImageFilter::bilinear,
                },
                rive::Mat2D::fromScale(128.0f, 128.0f) *
                    rive::Mat2D::fromTranslate(0.5f, 0.5f) *
                    rive::Mat2D::fromRotation(45.0f * float(M_PI) / 180.0f));

            p->shader(factory->makeLinearGradient(0.0f,
                                                  -100.0f,
                                                  0.0f,
                                                  100.0f,
                                                  gradientColors,
                                                  gradientStops,
                                                  std::size(gradientColors)));

            draw_oval(ren, {-220.0f, -220.0f, 220.0f, 220.0f}, p);

            p->modulatedImage(img1.get(),
                              {
                                  .wrapX = rive::ImageWrap::repeat,
                                  .wrapY = rive::ImageWrap::mirror,
                                  .filter = rive::ImageFilter::bilinear,
                              },
                              rive::Mat2D::fromScale(128.0f, 128.0f));
            p->color(0xFFFFA5F3);
            p->style(rive::RenderPaintStyle::stroke);
            p->thickness(30.0f);
            p->feather(30.0f);
            draw_oval(ren, {-220.0f, -220.0f, 220.0f, 220.0f}, p);
        }
        ren->restore();
    }
};
GMREGISTER(image_paint, return new ImagePaintGM)
