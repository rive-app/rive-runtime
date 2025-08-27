/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "common/rand.hpp"

using namespace rivegm;
using namespace rive;

constexpr int W = 1000;
constexpr int H = 400;
constexpr int N = 9999;

class VeryComplexGradGM : public GM
{
public:
    VeryComplexGradGM() : GM(W, H) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* renderer) override
    {
        Rand rand;
        ColorInt colors[N];
        float stops[N];
        for (size_t i = 0; i < N; ++i)
        {
            colors[i] = rand.u32() | 0xff808080;
            stops[i] = std::round(rand.f32() * W) / W;
        }
        std::sort(stops, stops + N);

        Paint paint;
        paint->shader(TestingWindow::Get()
                          ->factory()
                          ->makeLinearGradient(0, 0, W, 0, colors, stops, N));

        Path fullscreen = PathBuilder().addRect({0, 0, W, H}).detach();

        renderer->drawPath(fullscreen, paint);
    }
};

GMREGISTER(verycomplexgrad, return new VeryComplexGradGM())
