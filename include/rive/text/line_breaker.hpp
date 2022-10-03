/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_RENDER_GLYPH_LINE_H_
#define _RIVE_RENDER_GLYPH_LINE_H_

#include "rive/render_text.hpp"

namespace rive
{

enum class RenderTextAlign : uint8_t
{
    left = 0,
    right = 1,
    center = 2
};

struct RenderGlyphLine
{
    uint32_t startRun;
    uint32_t startIndex;
    uint32_t endRun;
    uint32_t endIndex;
    float startX;
    float top = 0, baseline = 0, bottom = 0;

    bool operator==(const RenderGlyphLine& o) const
    {
        return startRun == o.startRun && startIndex == o.startIndex && endRun == o.endRun &&
               endIndex == o.endIndex;
    }

    RenderGlyphLine() : startRun(0), startIndex(0), endRun(0), endIndex(0), startX(0.0f) {}
    RenderGlyphLine(uint32_t run, uint32_t index) :
        startRun(run), startIndex(index), endRun(run), endIndex(index), startX(0.0f)
    {}

    bool empty() const { return startRun == endRun && startIndex == endIndex; }
    static std::vector<RenderGlyphLine> BreakLines(Span<const RenderGlyphRun> runs,
                                                   float width,
                                                   RenderTextAlign align);

    // Compute values for top/baseline/bottom per line
    static void ComputeLineSpacing(rive::Span<RenderGlyphLine>,
                                   rive::Span<const RenderGlyphRun>,
                                   float width,
                                   RenderTextAlign align);
};

} // namespace rive

#endif
