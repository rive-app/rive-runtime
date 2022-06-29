#ifndef _RIVE_RENDER_GLYPH_LINE_H_
#define _RIVE_RENDER_GLYPH_LINE_H_

#include "rive/render_text.hpp"

namespace rive {

struct RenderGlyphLine {
  int startRun;
  int startIndex;
  int endRun;
  int endIndex;
  int wsRun;
  int wsIndex;
  float startX;
  float top = 0, baseline = 0, bottom = 0;

  RenderGlyphLine(int startRun,
                  int startIndex,
                  int endRun,
                  int endIndex,
                  int wsRun,
                  int wsIndex,
                  float startX) :
      startRun(startRun),
      startIndex(startIndex),
      endRun(endRun),
      endIndex(endIndex),
      wsRun(wsRun),
      wsIndex(wsIndex),
      startX(startX)
  {}

    static std::vector<RenderGlyphLine> BreakLines(Span<const RenderGlyphRun> runs,
                                                   Span<const int> breaks,
                                                   float width);
    // Compute vaues for top/baseline/bottom per line
    static void ComputeLineSpacing(rive::Span<RenderGlyphLine>,
                                   rive::Span<const RenderGlyphRun>);
};

} // namespace

#endif
