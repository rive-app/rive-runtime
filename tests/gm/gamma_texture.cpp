/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "common/write_png_file.hpp"
#include <vector>

constexpr int imgW = 512;
constexpr int imgH = 512;

// sRGB encoding function
uint8_t linearToSrgb(float linear)
{
    if (linear <= 0.0031308f)
        return static_cast<uint8_t>(std::round(255.0f * (12.92f * linear)));
    else
        return static_cast<uint8_t>(std::round(
            255.0f * (1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f)));
}

// Pack ARGB pixel
uint32_t packARGB(uint8_t r, uint8_t g, uint8_t b)
{
    return (255u << 24) | (r << 16) | (g << 8) | b;
}

std::vector<uint32_t> generateTexture()
{
    std::vector<uint32_t> pixels(imgW * imgH);

    for (int y = 0; y < imgH; ++y)
    {
        for (int x = 0; x < imgW; ++x)
        {
            uint8_t val = 0;

            if (y < 64)
            {
                // sRGB ramp
                float linear = static_cast<float>(x) / (imgW - 1);
                val = linearToSrgb(linear);
            }
            else if (y < 128)
            {
                // linear ramp
                val = static_cast<uint8_t>(std::round(255.0f * x / (imgW - 1)));
            }
            else if (y < 192)
            {
                // 8 sRGB squares
                int i = x / (imgW / 8);
                float linear = static_cast<float>(i) / 7.0f;
                val = linearToSrgb(linear);
            }
            else if (y < 256)
            {
                // 8 linear squares
                int i = x / (imgW / 8);
                val = static_cast<uint8_t>(std::round(255.0f * i / 7.0f));
            }
            else
            {
                if (x < imgW / 2)
                {
                    // checkerboard
                    val = ((x + y) % 2 == 0) ? 255 : 0;
                }
                else
                {
                    // right half blocks
                    val = (y < 256 + 128) ? 127 : linearToSrgb(0.5f); // ≈ 187
                }
            }

            int yD = (imgH - 1) - y;
            pixels[yD * imgW + x] = packARGB(val, val, val);
        }
    }

    return pixels;
}

using namespace rivegm;

class GammaTextureGM : public GM
{
public:
    GammaTextureGM() : GM(512, 768) {}

    void onDraw(rive::Renderer* renderer) override
    {
        auto pixData = generateTexture();
        std::vector<uint8_t> encodedData =
            EncodePNGToBuffer(imgW,
                              imgH,
                              (uint8_t*)pixData.data(),
                              PNGCompression::fast_rle);
        auto img = LoadImage(rive::Span<uint8_t>(encodedData));
        if (img == nullptr)
        {
            return;
        }

        // Full size image
        rive::AABB r = {0, 0, 512, 512};
        renderer->save();
        renderer->translate(0, 0);
        draw_image(renderer, img.get(), r);
        renderer->restore();

        // Below is 1/4 (1/2 * 1/2) version
        renderer->save();
        renderer->translate(0, 512);
        renderer->scale(.5, .5);
        draw_image(renderer, img.get(), r);
        renderer->restore();

        // Bottom right is 1/16 (1/4 * 1/4) version
        renderer->save();
        renderer->translate(256, 512);
        renderer->scale(.25, .25);
        draw_image(renderer, img.get(), r);
        renderer->restore();
    }
};
GMREGISTER(gamma_texture, return new GammaTextureGM)
