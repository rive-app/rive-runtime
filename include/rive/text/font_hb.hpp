/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_FONT_HB_HPP_
#define _RIVE_FONT_HB_HPP_

#include "rive/factory.hpp"
#include "rive/text.hpp"

struct hb_font_t;
struct hb_draw_funcs_t;

class HBFont : public rive::Font
{
    hb_draw_funcs_t* m_DrawFuncs;

public:
    hb_font_t* m_Font;

    // We assume ownership of font!
    HBFont(hb_font_t* font);
    ~HBFont() override;

    Axis getAxis(uint16_t index) const override;
    uint16_t getAxisCount() const override;
    std::vector<Coord> getCoords() const override;
    rive::rcp<rive::Font> makeAtCoords(rive::Span<const Coord>) const override;
    rive::RawPath getPath(rive::GlyphID) const override;
    rive::SimpleArray<rive::Paragraph> onShapeText(rive::Span<const rive::Unichar>,
                                                   rive::Span<const rive::TextRun>) const override;

    bool hasGlyph(rive::Span<const rive::Unichar>);

    static rive::rcp<rive::Font> Decode(rive::Span<const uint8_t>);

    // If the platform can supply fallback font(s), set this function pointer.
    // It will be called with a span of unichars, and the platform attempts to
    // return a font that can draw (at least some of) them. If no font is available
    // just return nullptr.

    using FallbackProc = rive::rcp<rive::Font> (*)(rive::Span<const rive::Unichar>);

    static FallbackProc gFallbackProc;
};

#endif
