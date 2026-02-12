/*
 * Copyright 2024 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"

#ifdef WITH_RIVE_TEXT

#include "assets/roboto_flex.ttf.hpp"
#include "assets/montserrat.ttf.hpp"
#include "common/testing_window.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text.hpp"

using namespace rivegm;

constexpr static char LOREM_IPSUM[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Etiam lorem "
    "magna, pharetra at eros a, malesuada vehicula ante. Cras pharetra metus "
    "eu augue venenatis, sed semper justo vulputate. Suspendisse a velit "
    "neque. In massa odio, auctor ac tincidunt ac, cursus sit amet sapien. "
    "Donec in sem vitae nisi commodo feugiat. Lorem ipsum dolor sit amet, "
    "consectetur adipiscing elit.";

class FeatherTextGM : public GM
{
public:
    FeatherTextGM(rive::Span<uint8_t> fontBytes, bool mirrored) :
        GM(1600, 1840),
        m_text(TestingWindow::Get()->factory()),
        m_mirrored(mirrored)
    {
        m_paint->color(0xff000000);
        m_text.maxWidth(720);
        m_text.sizing(rive::TextSizing::fixed);
        m_text.append(LOREM_IPSUM,
                      ref_rcp(m_paint.get()),
                      HBFont::Decode(fontBytes),
                      40.0f);
    }

    void updateFrameOptions(TestingWindow::FrameOptions* options) const override
    {
        // Force the RawText to clockwise.
        options->clockwiseFillOverride = true;
    }

    void onDraw(rive::Renderer* renderer) override
    {
        if (m_mirrored)
        {
            // Draw with a negative determinant to ensure the atlas accounts for
            // this in its signed coverage calculations.
            renderer->translate(width(), 0);
            renderer->scale(-1, 1);
        }

        renderer->translate(40, 40);

        for (int i = 0; i < 3; ++i)
        {
            m_paint->feather(powf(2.2f, i * 2 + 0) - .99999f);
            m_text.render(renderer);
            renderer->translate(800, 0);

            m_paint->feather(powf(2.2f, i * 2 + 1) - .99999f);
            m_text.render(renderer);
            renderer->translate(-800, 600);
        }
    }

private:
    rive::RawText m_text;
    Paint m_paint;
    const bool m_mirrored;
};

GMREGISTER(feathertext_roboto,
           return new FeatherTextGM(assets::roboto_flex_ttf(), false))
GMREGISTER(feathertext_montserrat,
           return new FeatherTextGM(assets::montserrat_ttf(), false))
GMREGISTER(feathertext_roboto_mirrored,
           return new FeatherTextGM(assets::roboto_flex_ttf(), true))
GMREGISTER(feathertext_montserrat_mirrored,
           return new FeatherTextGM(assets::montserrat_ttf(), true))

#endif
