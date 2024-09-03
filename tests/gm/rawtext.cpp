/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"

#ifdef WITH_RIVE_TEXT

#include "assets/roboto_flex.ttf.hpp"
#include "assets/montserrat.ttf.hpp"
#include "common/testing_window.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text.hpp"

using namespace rivegm;

class RawTextGM : public GM
{
public:
    RawTextGM() : GM(400, 335, "rawtext") {}

    void onDraw(rive::Renderer* renderer) override
    {
        auto roboto = HBFont::Decode(assets::roboto_flex_ttf());
        auto montserrat = HBFont::Decode(assets::montserrat_ttf());

        auto black = TestingWindow::Get()->factory()->makeRenderPaint();
        black->color(0xff000000);

        auto red = TestingWindow::Get()->factory()->makeRenderPaint();
        red->color(0x88ff0000);

        rive::RawText text(TestingWindow::Get()->factory());
        text.maxWidth(400);
        text.sizing(rive::TextSizing::fixed);
        text.append("Hello world!\n", black, roboto, 72.0f);
        text.append("Moon's cool too.\n", black, montserrat, 64.0f);
        text.append("Mars is red.\n", red, roboto, 72.0f);
        text.render(renderer);
    }
};

GMREGISTER(return new RawTextGM)

#endif
