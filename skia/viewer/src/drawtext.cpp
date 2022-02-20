/*
 * Copyright 2022 Rive
 */

#include "skia_factory.hpp"
#include "skia_renderer.hpp"
#include "skia_rive_fontmgr.hpp"

#include <string>

static void drawrun(rive::Factory* factory, rive::Renderer* renderer,
                    const rive::RenderGlyphRun& run, rive::Vec2D origin) {
    auto font = run.font.get();
    const auto scale = rive::Mat2D::fromScale(run.size, run.size);
    auto paint = factory->makeRenderPaint();
    paint->color(0xFFFFFFFF);

    for (size_t i = 0; i < run.glyphs.size(); ++i) {
        auto trans = rive::Mat2D::fromTranslate(origin.x + run.xpos[i], origin.y);
        auto rawpath = font->getPath(run.glyphs[i]);
        rawpath.transformInPlace(trans * scale);
        auto path = factory->makeRenderPath(rawpath.points(), rawpath.verbsU8(), rive::FillRule::nonZero);
        renderer->drawPath(path.get(), paint.get());
    }
}

struct UniString {
    std::vector<rive::Unichar> array;

    UniString(const char text[]) {
        while (*text) {
            array.push_back(*text++);
        }
    }
};

static std::string tagstr(uint32_t tag) {
    std::string s("abcd");
    s[0] = (tag >> 24) & 0xFF;
    s[1] = (tag >> 16) & 0xFF;
    s[2] = (tag >>  8) & 0xFF;
    s[3] = (tag >>  0) & 0xFF;
    return s;
}

void drawtext(rive::Factory* factory, rive::Renderer* renderer) {
    static std::vector<rive::RenderGlyphRun> gruns;

    if (gruns.size() == 0) {
        SkiaRiveFontMgr fmgr;

        auto font0 = fmgr.findFont("Times New Roman");
        auto font1 = fmgr.findFont("Skia");

        if (false) {
            auto axes = font1->getAxes();
            for (auto a : axes) {
                printf("%s %g %g %g\n", tagstr(a.tag).c_str(), a.min, a.def, a.max);
            }
            auto coords = font1->getCoords();
            for (auto c : coords) {
                printf("%s %g\n", tagstr(c.axis).c_str(), c.value);
            }
        }

        rive::RenderFont::Coord c0 = {'wght', 0.5f},
                                c1 = {'wght', 2.0f};

        UniString str("Uneasy lies the head that wears a crown.");
        rive::RenderTextRun truns[] = {
            { font0, 60, 1 },
            { font0, 30, 6 },
            { font1->makeAtCoord(c0), 30, 4 },
            { font1, 30, 4 },
            { font1->makeAtCoord(c1), 30, 5 },
            { font0, 30, 20 },
        };

        gruns = fmgr.shapeText(rive::toSpan(str.array), rive::Span(truns, 6));
    }

    renderer->save();
    renderer->scale(3, 3);
    for (const auto& g : gruns) {
        drawrun(factory, renderer, g, {10, 50});
    }
    renderer->restore();
}

