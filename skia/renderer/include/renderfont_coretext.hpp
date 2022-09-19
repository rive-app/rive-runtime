/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_RENDERFONT_CORETEXT_HPP_
#define _RIVE_RENDERFONT_CORETEXT_HPP_

#include "rive/factory.hpp"
#include "rive/render_text.hpp"

#if defined(RIVE_BUILD_FOR_OSX)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(RIVE_BUILD_FOR_IOS)
#include <CoreText/CoreText.h>
#endif

class CoreTextRenderFont : public rive::RenderFont {
public:
    CTFontRef m_font;
    const std::vector<Axis> m_axes;
    const std::vector<Coord> m_coords;

    // We assume ownership of font!
    CoreTextRenderFont(CTFontRef, std::vector<Axis>);
    ~CoreTextRenderFont() override;

    std::vector<Axis> getAxes() const override { return m_axes; }
    std::vector<Coord> getCoords() const override { return m_coords; }
    rive::rcp<rive::RenderFont> makeAtCoords(rive::Span<const Coord>) const override;
    rive::RawPath getPath(rive::GlyphID) const override;
    rive::SimpleArray<rive::RenderGlyphRun>
        onShapeText(rive::Span<const rive::Unichar>,
                    rive::Span<const rive::RenderTextRun>) const override;

    static rive::rcp<rive::RenderFont> Decode(rive::Span<const uint8_t>);
    static rive::rcp<rive::RenderFont> FromCT(CTFontRef);
};

#endif
