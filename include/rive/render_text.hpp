/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_RENDER_TEXT_HPP_
#define _RIVE_RENDER_TEXT_HPP_

#include "rive/math/raw_path.hpp"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"

namespace rive {

using Unichar = int32_t;
using GlyphID = uint16_t;

class RenderFont : public RefCnt {
public:
    // This is experimental
    // -- may only be needed by Editor
    // -- so it may be removed from here later
    //
    struct Axis {
        uint32_t    tag;
        float       min;
        float       def;    // default value
        float       max;
    };

    // Returns the canonical set of Axes for this font. Use this to know
    // what variations are possible. If you want to know the specific
    // coordinate within that variations space for *this* font, call
    // getCoords().
    //
    virtual std::vector<Axis> getAxes() const = 0;

    struct Coord {
        uint32_t    axis;
        float       value;
    };

    // Returns the specific coords in variation space for this font.
    // If you want to have a description of the entire variation space,
    // call getAxes().
    //
    virtual std::vector<Coord> getCoords() const = 0;

    virtual rcp<RenderFont> makeAtCoords(Span<const Coord>) const = 0;

    rcp<RenderFont> makeAtCoord(Coord c) {
        return this->makeAtCoords(Span<const Coord>(&c, 1));
    }

    // Returns a 1-point path for this glyph. It will be positioned
    // relative to (0,0) with the typographic baseline at y = 0.
    //
    virtual RawPath getPath(GlyphID) const = 0;
};

struct RenderTextRun {
    rcp<RenderFont> font;
    float           size;
    uint32_t        unicharCount;
};

struct RenderGlyphRun {
    rcp<RenderFont>         font;
    float                   size;
    uint32_t                startTextIndex;

    std::vector<GlyphID>    glyphs;
    std::vector<float>      xpos;   // xpos.size() == glyphs.size() + 1
};

} // namespace rive
#endif
