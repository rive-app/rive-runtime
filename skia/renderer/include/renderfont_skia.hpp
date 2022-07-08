/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_RENDERFONT_SKIA_HPP_
#define _RIVE_RENDERFONT_SKIA_HPP_

#include "rive/render_text.hpp"
#include "include/core/SkTypeface.h"

class SkiaRenderFont : public rive::RenderFont {
public:
    sk_sp<SkTypeface> m_Typeface;

    SkiaRenderFont(sk_sp<SkTypeface>);

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
