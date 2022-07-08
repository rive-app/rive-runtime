/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_RENDERFONT_HB_HPP_
#define _RIVE_RENDERFONT_HB_HPP_

#include "rive/factory.hpp"
#include "rive/render_text.hpp"

struct hb_font_t;
struct hb_draw_funcs_t;

class HBRenderFont : public rive::RenderFont {
    hb_draw_funcs_t* m_DrawFuncs;

public:
    hb_font_t* m_Font;

    // We assume ownership of font!
    HBRenderFont(hb_font_t* font);
    ~HBRenderFont() override;

    std::vector<Axis> getAxes() const override;
    std::vector<Coord> getCoords() const override;
    rive::rcp<rive::RenderFont> makeAtCoords(rive::Span<const Coord>) const override;
    rive::RawPath getPath(rive::GlyphID) const override;
    std::vector<rive::RenderGlyphRun>
        onShapeText(rive::Span<const rive::Unichar>,
                    rive::Span<const rive::RenderTextRun>) const override;

    static rive::rcp<rive::RenderFont> Decode(rive::Span<const uint8_t>);
};

#endif
