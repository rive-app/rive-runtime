/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_SKIA_FONTMGR_HPP_
#define _RIVE_SKIA_FONTMGR_HPP_

#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include "rive/render_text.hpp"

// This is a working model for how we might extend Factory.hpp
class SkiaRiveFontMgr {
public:
    rive::rcp<rive::RenderFont> decodeFont(rive::Span<const uint8_t>);
    rive::rcp<rive::RenderFont> findFont(const char name[]);
    std::vector<rive::RenderGlyphRun> shapeText(rive::Span<const rive::Unichar> text,
                                                rive::Span<const rive::RenderTextRun> runs);

};

#endif
