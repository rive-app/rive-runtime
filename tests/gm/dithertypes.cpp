/*
 * Copyright 2024 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"

#ifdef WITH_RIVE_TEXT

#include "assets/roboto_flex.ttf.hpp"
#include "common/testing_window.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text.hpp"

using namespace rive;
using namespace rivegm;

// Validate all our dither types by rendering feathered strokes and fills
DEF_SIMPLE_GM_WITH_CLEAR_COLOR(dithertypes, 0x80000000, 1600, 800, renderer)
{
    gpu::RenderContext* renderContext = TestingWindow::Get()->renderContext();
    gpu::RenderContext::FrameDescriptor frameDescriptor;
    if (renderContext != nullptr)
    {
        frameDescriptor = renderContext->frameDescriptor();
        frameDescriptor.loadAction = gpu::LoadAction::preserveRenderTarget;
    }

    Paint textPaint;
    textPaint->color((0xff << 24) | (211 << 16) | (251 << 8) | 229);
    textPaint->feather(1800);

    rive::RawText text(TestingWindow::Get()->factory());
    text.maxWidth(500);
    text.sizing(rive::TextSizing::fixed);
    text.append("@",
                ref_rcp(textPaint.get()),
                HBFont::Decode(assets::roboto_flex_ttf()),
                2600.0f);

    for (int i = 0; i < 2; ++i)
    {
        TestingWindow::Get()->flushPLSContext();

        switch (i)
        {
            case 0:
                frameDescriptor.ditherMode = rive::gpu::DitherMode::none;
                break;
            case 1:
                frameDescriptor.ditherMode =
                    rive::gpu::DitherMode::interleavedGradientNoise;
                break;
            default:
                break;
        }
        renderContext->beginFrame(frameDescriptor);

        renderer->save();
        renderer->translate(0, 0);
        renderer->translate((i % 2) * 800, int(i / 2) * 800);

        Path rect;
        rect->addRect(00, 00, 750, 750);
        renderer->clipPath(rect);
        text.render(renderer);
        renderer->restore();
    }
}

#endif
