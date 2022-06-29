/*
 * Copyright 2022 Rive
 */

#include "viewer_content.hpp"

#include "rive/refcnt.hpp"
#include "rive/render_text.hpp"

#include "skia_factory.hpp"
#include "skia_renderer.hpp"
#include "line_breaker.hpp"

static bool ws(rive::Unichar c) {
    return c <= ' ';
}

std::vector<int> compute_word_breaks(rive::Span<rive::Unichar> chars) {
    std::vector<int> breaks;

    const unsigned len = chars.size();
    for (unsigned i = 0; i < len;) {
      // skip ws
      while (i < len && ws(chars[i])) {
        ++i;
      }
      breaks.push_back(i); // word start
      // skip non-ws
      while (i < len && !ws(chars[i])) {
        ++i;
      }
      breaks.push_back(i); // word end
    }
    assert(breaks[breaks.size()-1] == len);
    return breaks;
}

static void drawrun(rive::Factory* factory, rive::Renderer* renderer,
                    const rive::RenderGlyphRun& run, unsigned startIndex, unsigned endIndex, rive::Vec2D origin) {
    auto font = run.font.get();
    const auto scale = rive::Mat2D::fromScale(run.size, run.size);
    auto paint = factory->makeRenderPaint();
    paint->color(0xFFFFFFFF);

    assert(startIndex >= 0 && endIndex <= run.glyphs.size());
    for (size_t i = startIndex; i < endIndex; ++i) {
        auto trans = rive::Mat2D::fromTranslate(origin.x + run.xpos[i], origin.y);
        auto rawpath = font->getPath(run.glyphs[i]);
        rawpath.transformInPlace(trans * scale);
        auto path = factory->makeRenderPath(rawpath.points(), rawpath.verbsU8(), rive::FillRule::nonZero);
        renderer->drawPath(path.get(), paint.get());
    }
}

static void drawpara(rive::Factory* factory, rive::Renderer* renderer,
                     rive::Span<const rive::RenderGlyphLine> lines,
                     rive::Span<const rive::RenderGlyphRun> runs,
                     rive::Vec2D origin) {
    for (const auto& line : lines) {
        const float x0 = runs[line.startRun].xpos[line.startIndex];
        int startGIndex = line.startIndex;
        for (int runIndex = line.startRun; runIndex <= line.endRun; ++runIndex) {
            const auto& run = runs[runIndex];
            int endGIndex = runIndex == line.endRun ? line.endIndex : run.glyphs.size();
            drawrun(factory, renderer, run, startGIndex, endGIndex,
                    {origin.x - x0, origin.y + line.baseline});
            startGIndex = 0;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////

typedef rive::rcp<rive::RenderFont> (*RenderFontFactory)(rive::Span<const uint8_t>);

#include "renderfont_skia.hpp"
#include "renderfont_hb.hpp"
#include "include/core/SkData.h"

static void draw_line(rive::Factory* factory, rive::Renderer* renderer, float x) {
    auto paint = factory->makeRenderPaint();
    paint->style(rive::RenderPaintStyle::stroke);
    paint->thickness(1);
    paint->color(0xFFFFFFFF);
    auto path = factory->makeEmptyRenderPath();
    path->move({x, 0});
    path->line({x, 1000});
    renderer->drawPath(path.get(), paint.get());
}

static rive::SkiaFactory skiaFactory;

static rive::RenderTextRun append(std::vector<rive::Unichar>* unichars,
                                  rive::rcp<rive::RenderFont> font,
                                  float size, const char text[]) {
    uint32_t n = 0;
    while (text[n]) {
        unichars->push_back(text[n]);   // todo: utf8 -> unichar
        n += 1;
    }
    return { std::move(font), size, n };
}

class TextContent : public ViewerContent {
    std::vector<rive::Unichar> m_unichars;
    std::vector<int> m_breaks;
    std::vector<rive::RenderTextRun> m_truns;
    std::vector<rive::RenderGlyphRun> m_gruns;

    void make_truns() {
        auto loader = [](const char filename[]) -> rive::rcp<rive::RenderFont> {
            auto bytes = ViewerContent::LoadFile(filename);
            if (bytes.size() == 0) {
                return nullptr;
            }
            return HBRenderFont::Decode(rive::toSpan(bytes));
        };

        const char* fontFiles[] = {
            "../../test/assets/RobotoFlex.ttf",
            "../../test/assets/LibreBodoni-Italic-VariableFont_wght.ttf",
        };

        auto font0 = loader(fontFiles[0]);
        auto font1 = loader(fontFiles[1]);
        assert(font0);
        assert(font1);

        rive::RenderFont::Coord c1 = {'wght', 100.f},
                                c2 = {'wght', 700.f};

        m_truns.push_back(append(&m_unichars, font0->makeAtCoord(c2), 60, "U"));
        m_truns.push_back(append(&m_unichars, font0->makeAtCoord(c1), 30, "neasy"));
        m_truns.push_back(append(&m_unichars, font1, 30, " fits the crown"));
        m_truns.push_back(append(&m_unichars, font1->makeAtCoord(c1), 30, " that often"));
        m_truns.push_back(append(&m_unichars, font0, 30, " lies the head."));

        m_breaks = compute_word_breaks(rive::toSpan(m_unichars));
    }

    void make_gruns() {
        m_gruns = m_truns[0].font->shapeText(rive::toSpan(m_unichars), rive::toSpan(m_truns));
    }

public:
    TextContent() {
        this->make_truns();
    }

    void handleDraw(SkCanvas* canvas, double) override {
        rive::SkiaRenderer renderer(canvas);

        this->make_gruns();

        renderer.save();
        renderer.translate(10, 0);

        renderer.save();
        renderer.scale(3, 3);

        static float width = 300;
        static float dw = 1;
        width += dw; if (width > 600) { dw = -dw; } if (width < 50) { dw = -dw; }
        auto lines = rive::RenderGlyphLine::BreakLines(rive::toSpan(m_gruns), rive::toSpan(m_breaks), width);

        drawpara(&skiaFactory, &renderer, rive::toSpan(lines), rive::toSpan(m_gruns), {0, 0});

        draw_line(&skiaFactory, &renderer, width);

        renderer.restore();

        renderer.restore();
    }

    void handleResize(int width, int height) override {}
    void handleImgui() override {}
};

std::unique_ptr<ViewerContent> ViewerContent::Text(const char filename[]) {
    return std::make_unique<TextContent>();
}
