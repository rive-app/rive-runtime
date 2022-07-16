/*
 * Copyright 2022 Rive
 */
#ifdef RIVE_TEXT
#include "rive/factory.hpp"
#include "rive/render_text.hpp"
#include "renderfont_skia.hpp"

#include "include/core/SkData.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkPath.h"
#include "include/core/SkTypeface.h"

static void setupFont(SkFont* font) {
    font->setLinearMetrics(true);
    font->setBaselineSnap(false);
    font->setHinting(SkFontHinting::kNone);
}

static rive::RenderFont::LineMetrics make_lmx(sk_sp<SkTypeface> tf) {
    SkFont font(tf, 1.0f);
    setupFont(&font);
    SkFontMetrics metrics;
    (void)font.getMetrics(&metrics);
    return {metrics.fAscent, metrics.fDescent};
}

SkiaRenderFont::SkiaRenderFont(sk_sp<SkTypeface> tf) :
    RenderFont(make_lmx(tf)), m_Typeface(std::move(tf)) {}

rive::rcp<rive::RenderFont> SkiaRenderFont::Decode(rive::Span<const uint8_t> span) {
    auto tf = SkTypeface::MakeFromData(SkData::MakeWithCopy(span.data(), span.size()));
    return tf ? rive::rcp<rive::RenderFont>(new SkiaRenderFont(std::move(tf))) : nullptr;
}

std::vector<rive::RenderFont::Axis> SkiaRenderFont::getAxes() const {
    std::vector<rive::RenderFont::Axis> axes;
    const int count = m_Typeface->getVariationDesignParameters(nullptr, 0);
    if (count > 0) {
        std::vector<SkFontParameters::Variation::Axis> src(count);
        (void)m_Typeface->getVariationDesignParameters(src.data(), count);
        axes.resize(count);
        for (int i = 0; i < count; ++i) {
            axes[i] = {src[i].tag, src[i].min, src[i].def, src[i].max};
        }
    }
    return axes;
}

std::vector<rive::RenderFont::Coord> SkiaRenderFont::getCoords() const {
    int count = m_Typeface->getVariationDesignPosition(nullptr, 0);
    std::vector<SkFontArguments::VariationPosition::Coordinate> skcoord(count);
    m_Typeface->getVariationDesignPosition(skcoord.data(), count);

    std::vector<rive::RenderFont::Coord> coords(count);
    for (int i = 0; i < count; ++i) {
        coords[i] = {skcoord[i].axis, skcoord[i].value};
    }
    return coords;
}

rive::rcp<rive::RenderFont> SkiaRenderFont::makeAtCoords(rive::Span<const Coord> coords) const {
    const int count = (int)coords.size();
    SkAutoSTArray<16, SkFontArguments::VariationPosition::Coordinate> storage(count);
    for (size_t i = 0; i < count; ++i) {
        storage[i].axis = coords[i].axis;
        storage[i].value = coords[i].value;
    }
    SkFontArguments args;
    args.setVariationDesignPosition({storage.get(), count});

    auto face = m_Typeface->makeClone(args);

    if (face->uniqueID() == m_Typeface->uniqueID()) {
        auto self = const_cast<SkiaRenderFont*>(this);
        return rive::rcp<rive::RenderFont>(rive::safe_ref(self));
    } else {
        return rive::rcp<rive::RenderFont>(new SkiaRenderFont(std::move(face)));
    }
}

static inline rive::Vec2D rv(SkPoint p) { return rive::Vec2D(p.fX, p.fY); }

rive::RawPath SkiaRenderFont::getPath(rive::GlyphID glyph) const {
    SkFont font(m_Typeface, 1.0f);
    setupFont(&font);

    SkPath skpath;
    font.getPath(glyph, &skpath);

    rive::RawPath rpath;
    SkPath::RawIter iter(skpath);
    SkPoint pts[4];
    bool done = false;
    while (!done) {
        switch (iter.next(pts)) {
            case SkPath::kMove_Verb:
                rpath.move(rv(pts[0]));
                break;
            case SkPath::kLine_Verb:
                rpath.line(rv(pts[1]));
                break;
            case SkPath::kQuad_Verb:
                rpath.quad(rv(pts[1]), rv(pts[2]));
                break;
            case SkPath::kConic_Verb:
                rpath.quad(rv(pts[1]), rv(pts[2]));
                break; // TODO: convert
            case SkPath::kCubic_Verb:
                rpath.cubic(rv(pts[1]), rv(pts[2]), rv(pts[3]));
                break;
            case SkPath::kClose_Verb:
                rpath.close();
                break;
            case SkPath::kDone_Verb:
                done = true;
                break;
        }
    }
    return rpath;
}

///////////////////////////////////////////////////////////

static float shapeRun(rive::RenderGlyphRun* grun,
                      const rive::RenderTextRun& trun,
                      const rive::Unichar text[],
                      size_t textOffset,
                      float origin) {
    const int glyphCount = SkToInt(trun.unicharCount); // simple shaper, no ligatures

    grun->font = trun.font;
    grun->size = trun.size;

    grun->glyphs.resize(glyphCount);
    grun->textOffsets.resize(glyphCount);
    grun->xpos.resize(glyphCount + 1);

    SkiaRenderFont* rfont = static_cast<SkiaRenderFont*>(trun.font.get());
    rfont->m_Typeface->unicharsToGlyphs(text + textOffset, glyphCount, grun->glyphs.data());

    // simple shaper, assume one glyph per char
    for (int i = 0; i < glyphCount; ++i) {
        grun->textOffsets[i] = textOffset + i;
    }

    SkFont font(rfont->m_Typeface, grun->size);
    setupFont(&font);

    // We get 'widths' from skia, but then turn them into xpos
    // this will write count values, but xpos has count+1 slots
    font.getWidths(grun->glyphs.data(), glyphCount, grun->xpos.data());
    for (auto& xp : grun->xpos) {
        auto width = xp;
        xp = origin;
        origin += width;
    }

    return grun->xpos.data()[glyphCount];
}

std::vector<rive::RenderGlyphRun>
SkiaRenderFont::onShapeText(rive::Span<const rive::Unichar> text,
                            rive::Span<const rive::RenderTextRun> truns) const {
    std::vector<rive::RenderGlyphRun> gruns(truns.size());

    int i = 0;
    size_t offset = 0;
    float origin = 0;
    for (const auto& tr : truns) {
        origin = shapeRun(&gruns[i], tr, text.data(), offset, origin);
        offset += tr.unicharCount;
        i += 1;
    }

    return gruns;
}
#endif