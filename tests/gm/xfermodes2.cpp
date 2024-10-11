/*
 * Copyright 2013 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"

using namespace rive;
using namespace rivegm;

class Xfermodes2GM : public GM
{
public:
    Xfermodes2GM() : GM(455, 475, "xfermodes2") {}

protected:
    rive::ColorInt clearColor() const override { return 0; } // TRANSPARENT!

    void onDraw(rive::Renderer* renderer) override
    {
        renderer->translate(10, 20);

        const float w = kSize;
        const float h = kSize;

        rive::Factory* factory = TestingWindow::Get()->factory();

        float stops[2] = {0, 1};
        rive::ColorInt dstColors[2] = {0, 0xff00ff00};
        rive::ColorInt srcColors[2] = {0, 0xffff0000};
        rcp<RenderShader> dstGrad =
            factory->makeLinearGradient(0, 0, w, 0, dstColors, stops, 2);
        rcp<RenderShader> srcGrad =
            factory->makeLinearGradient(0, 0, 0, h, srcColors, stops, 2);

        // SkFont font(ToolUtils::create_portable_typeface());

        const int W = 6;

        float x = 0, y = 0;
        size_t m = 0;

        for (BlendMode mode : {BlendMode::srcOver,
                               BlendMode::screen,
                               BlendMode::overlay,
                               BlendMode::darken,
                               BlendMode::lighten,
                               BlendMode::colorDodge,
                               BlendMode::colorBurn,
                               BlendMode::hardLight,
                               BlendMode::softLight,
                               BlendMode::difference,
                               BlendMode::exclusion,
                               BlendMode::multiply,
                               BlendMode::hue,
                               BlendMode::saturation,
                               BlendMode::color,
                               BlendMode::luminosity})
        {
            renderer->save();

            renderer->translate(x, y);
            auto p = factory->makeRenderPaint();
            // // p.setStyle(SkPaint::kFill_Style);
            // p.setShader(fBG);
            rive::AABB r = {0, 0, w, h};
            // rivegm::draw_rect(renderer, r, p.get());

            p->shader(dstGrad);
            rivegm::draw_rect(renderer, r, p.get());
            p->shader(srcGrad);
            p->blendMode(mode);
            rivegm::draw_rect(renderer, r, p.get());

            r.inset(-.5f, -.5f);
            p->style(RenderPaintStyle::stroke);
            p->shader(nullptr);
            p->blendMode(BlendMode::srcOver);
            rivegm::draw_rect(renderer, r, p.get());

            renderer->restore();

#if 0
            SkTextUtils::DrawString(renderer,
                                    SkBlendMode_Name(mode),
                                    x + w / 2,
                                    y - font.getSize() / 2,
                                    font,
                                    SkPaint(),
                                    SkTextUtils::kCenter_Align);
#endif
            x += w + 10;
            if ((m % W) == W - 1)
            {
                x = 0;
                y += h + 30;
            }
            ++m;
        }
    }

private:
    enum
    {
        kShift = 2,
        kSize = 256 >> kShift,
    };
};

//////////////////////////////////////////////////////////////////////////////

GMREGISTER(return new Xfermodes2GM)
