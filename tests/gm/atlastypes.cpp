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
#include "common/rand.hpp"

#ifndef RIVE_TOOLS_NO_GL
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#endif

using namespace rive;
using namespace rivegm;

// Validate all our atlas fallbacks by rendering feathered strokes and fills
// with every AtlasType.
DEF_SIMPLE_GM_WITH_CLEAR_COLOR(atlastypes, 0x80000000, 1600, 1600, renderer)
{
    gpu::RenderContext* renderContext = TestingWindow::Get()->renderContext();
    gpu::RenderContext::FrameDescriptor frameDescriptor;
    if (renderContext != nullptr)
    {
        frameDescriptor = renderContext->frameDescriptor();
        frameDescriptor.loadAction = gpu::LoadAction::preserveRenderTarget;
    }

    Paint textPaint;
    textPaint->color(0xffffffff);
    textPaint->feather(30);

    rive::RawText text(TestingWindow::Get()->factory());
    text.maxWidth(800);
    text.sizing(rive::TextSizing::fixed);
    text.append("RIVE",
                ref_rcp(textPaint.get()),
                HBFont::Decode(assets::roboto_flex_ttf()),
                325.0f);

    Paint starPaint;
    starPaint->color(0x80ffff80);
    starPaint->blendMode(BlendMode::colorBurn);

    Paint starStroke;
    starStroke->color(0x80ffffff);
    starStroke->blendMode(BlendMode::colorDodge);
    starStroke->style(RenderPaintStyle::stroke);
    starStroke->thickness(.05f);

    for (int i = 0; i < 6; ++i)
    {
        if (renderContext != nullptr)
        {
#ifndef RIVE_TOOLS_NO_GL
            if (gpu::RenderContextGLImpl* glImpl =
                    TestingWindow::Get()->renderContextGLImpl())
            {
                TestingWindow::Get()->flushPLSContext();
                glImpl->testingOnly_resetAtlasDesiredType(
                    renderContext,
                    static_cast<gpu::RenderContextGLImpl::AtlasType>(i));
                renderContext->beginFrame(frameDescriptor);
            }
#endif
        }

        renderer->save();
        renderer->translate(60, 100);
        renderer->translate((i % 2) * 800, int(i / 2) * 500);

        text.render(renderer);

        Rand rando;
        for (size_t i = 0; i < 30; ++i)
        {
            Path star;
            star->fillRule(FillRule::clockwise);
            rivegm::path_add_star(star,
                                  rando.u32(2, 4) * 2 + 1,
                                  rando.f32(),
                                  1);
            renderer->save();
            renderer->translate(rando.f32(-50, 750), rando.f32(-50, 350));
            float s = rando.f32(50, 100);
            renderer->scale(s, s);
            float f = rando.f32(.1f, .5f);
            starPaint->feather(f);
            starStroke->feather(f);
            renderer->drawPath(star, starPaint);
            renderer->drawPath(star, starStroke);
            renderer->restore();
        }
        renderer->restore();
    }

    if (renderContext != nullptr)
    {
        // Restore the most ideal AtlasType for future tests.
#ifndef RIVE_TOOLS_NO_GL
        if (gpu::RenderContextGLImpl* glImpl =
                TestingWindow::Get()->renderContextGLImpl())
        {
            TestingWindow::Get()->flushPLSContext();
            glImpl->testingOnly_resetAtlasDesiredType(
                renderContext,
                gpu::RenderContextGLImpl::AtlasType::r32f);
            renderContext->beginFrame(frameDescriptor);
        }
#endif
    }
}

#endif
