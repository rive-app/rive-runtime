/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_RENDER_TEXT_HPP_
#define _RIVE_RENDER_TEXT_HPP_

#include "rive/math/raw_path.hpp"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include "rive/simple_array.hpp"

namespace rive {

using Unichar = uint32_t;
using GlyphID = uint16_t;

struct RenderTextRun;
struct RenderGlyphRun;

class RenderFont : public RefCnt<RenderFont> {
public:
    virtual ~RenderFont() {}

    struct LineMetrics {
        float ascent, descent;
    };

    const LineMetrics& lineMetrics() const { return m_LineMetrics; }

    // This is experimental
    // -- may only be needed by Editor
    // -- so it may be removed from here later
    //
    struct Axis {
        uint32_t tag;
        float min;
        float def; // default value
        float max;
    };

    // Returns the canonical set of Axes for this font. Use this to know
    // what variations are possible. If you want to know the specific
    // coordinate within that variations space for *this* font, call
    // getCoords().
    //
    virtual std::vector<Axis> getAxes() const = 0;

    struct Coord {
        uint32_t axis;
        float value;
    };

    // Returns the specific coords in variation space for this font.
    // If you want to have a description of the entire variation space,
    // call getAxes().
    //
    virtual std::vector<Coord> getCoords() const = 0;

    virtual rcp<RenderFont> makeAtCoords(Span<const Coord>) const = 0;

    rcp<RenderFont> makeAtCoord(Coord c) { return this->makeAtCoords(Span<const Coord>(&c, 1)); }

    // Returns a 1-point path for this glyph. It will be positioned
    // relative to (0,0) with the typographic baseline at y = 0.
    //
    virtual RawPath getPath(GlyphID) const = 0;

    rive::SimpleArray<RenderGlyphRun> shapeText(rive::Span<const rive::Unichar> text,
                                                rive::Span<const rive::RenderTextRun> runs) const;

protected:
    RenderFont(const LineMetrics& lm) : m_LineMetrics(lm) {}

    virtual rive::SimpleArray<RenderGlyphRun>
    onShapeText(rive::Span<const rive::Unichar> text,
                rive::Span<const rive::RenderTextRun> runs) const = 0;

private:
    const LineMetrics m_LineMetrics;
};

struct RenderTextRun {
    rcp<RenderFont> font;
    float size;
    uint32_t unicharCount;
};

struct RenderGlyphRun {
    RenderGlyphRun(size_t glyphCount = 0) :
        glyphs(glyphCount), textIndices(glyphCount), xpos(glyphCount + 1) {}

    RenderGlyphRun(rive::SimpleArray<GlyphID> glyphIds,
                   rive::SimpleArray<uint32_t> offsets,
                   rive::SimpleArray<float> xs) :
        glyphs(glyphIds), textIndices(offsets), xpos(xs) {}

    rcp<RenderFont> font;
    float size;
    // List of glyphs, represented by font specific glyph ids. Length is equal to number of glyphs
    // in the run.
    rive::SimpleArray<GlyphID> glyphs;

    // Index in the unicode text array representing the text displayed in this run. Because each
    // glyph can be composed of multiple unicode values, this index points to the first index in the
    // unicode text. Length is equal to number of glyphs in the run.
    rive::SimpleArray<uint32_t> textIndices;

    // X position of each glyph, with an extra value at the end for the right most extent of the
    // last glyph.
    rive::SimpleArray<float> xpos;

    // List of possible indices to line break at. Has a stride of 2 uint32_ts where each pair marks
    // the start and end of a word, with the exception of a return character (forced linebreak)
    // which is represented as a 0 length word (where start/end index is the same).
    rive::SimpleArray<uint32_t> breaks;
};

} // namespace rive
#endif
