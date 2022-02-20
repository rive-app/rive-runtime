/*
 * Copyright 2022 Rive
 */

#include "rive/factory.hpp"
#include "rive/render_text.hpp"
#include "skia_rive_fontmgr.hpp"

#include "include/core/SkTypeface.h"
#include "include/core/SkFont.h"
#include "include/core/SkPath.h"

class SkiaRenderFont : public rive::RenderFont {
public:
    sk_sp<SkTypeface> m_Typeface;

    SkiaRenderFont(sk_sp<SkTypeface> tf) : m_Typeface(std::move(tf)) {}

    std::vector<Axis> getAxes() const override;
    std::vector<Coord> getCoords() const override;
    rive::rcp<rive::RenderFont> makeAtCoords(rive::Span<const Coord>) const override;
    rive::RawPath getPath(rive::GlyphID) const override;
};

std::vector<rive::RenderFont::Axis> SkiaRenderFont::getAxes() const {
    std::vector<rive::RenderFont::Axis> axes;
    const int count = m_Typeface->getVariationDesignParameters(nullptr, 0);
    if (count > 0) {
        std::vector<SkFontParameters::Variation::Axis> src(count);
        (void)m_Typeface->getVariationDesignParameters(src.data(), count);
        axes.resize(count);
        for (int i = 0; i < count; ++i) {
            axes[i] = { src[i].tag, src[i].min, src[i].def, src[i].max };
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
        coords[i] = { skcoord[i].axis, skcoord[i].value };
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

static void setupFont(SkFont* font) {
    font->setLinearMetrics(true);
    font->setBaselineSnap(false);
    font->setHinting(SkFontHinting::kNone);
}

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
            case SkPath::kMove_Verb:  rpath.move (rv(pts[0])); break;
            case SkPath::kLine_Verb:  rpath.line (rv(pts[1])); break;
            case SkPath::kQuad_Verb:  rpath.quad (rv(pts[1]), rv(pts[2])); break;
            case SkPath::kConic_Verb: rpath.quad (rv(pts[1]), rv(pts[2])); break;  // TODO: convert
            case SkPath::kCubic_Verb: rpath.cubic(rv(pts[1]), rv(pts[2]), rv(pts[3])); break;
            case SkPath::kClose_Verb: rpath.close(); break;
            case SkPath::kDone_Verb: done = true; break;
        }
    }
    return rpath;
}

///////////////////////////////////////////////////////////

rive::rcp<rive::RenderFont> SkiaRiveFontMgr::decodeFont(rive::Span<const uint8_t> data) {
    auto tf = SkTypeface::MakeDefault(); // ignoring data for now
    return rive::rcp<rive::RenderFont>(new SkiaRenderFont(std::move(tf)));
}

rive::rcp<rive::RenderFont> SkiaRiveFontMgr::findFont(const char name[]) {
    auto tf = SkTypeface::MakeFromName(name, SkFontStyle());
    if (!tf) {
        tf = SkTypeface::MakeDefault();
    }
    return rive::rcp<rive::RenderFont>(new SkiaRenderFont(std::move(tf)));
}

static float shapeRun(rive::RenderGlyphRun* grun, const rive::RenderTextRun& trun,
                      const rive::Unichar text[], size_t textOffset, float origin) {
    grun->font = trun.font;
    grun->size = trun.size;
    grun->startTextIndex = textOffset;
    grun->glyphs.resize(trun.unicharCount);
    grun->xpos.resize(trun.unicharCount + 1);

    const int count = SkToInt(trun.unicharCount);

    SkiaRenderFont* rfont = static_cast<SkiaRenderFont*>(trun.font.get());
    rfont->m_Typeface->unicharsToGlyphs(text + textOffset, count, grun->glyphs.data());

    SkFont font(rfont->m_Typeface, grun->size);
    setupFont(&font);

    // We get 'widths' from skia, but then turn them into xpos
    // this will write count values, but xpos has count+1 slots
    font.getWidths(grun->glyphs.data(), count, grun->xpos.data());
    for (auto& xp : grun->xpos) {
        auto width = xp;
        xp = origin;
        origin += width;
    }

    return grun->xpos.data()[count];
}

std::vector<rive::RenderGlyphRun>
SkiaRiveFontMgr::shapeText(rive::Span<const rive::Unichar> text,
                           rive::Span<const rive::RenderTextRun> truns) {
    std::vector<rive::RenderGlyphRun> gruns;

    // sanity check
    size_t count = 0;
    for (const auto& tr : truns) {
        count += tr.unicharCount;
    }
    if (count > text.size()) {
        return gruns;   // not enough text, so abort
    }

    gruns.resize(truns.size());
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
