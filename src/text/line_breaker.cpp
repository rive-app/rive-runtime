/*
 * Copyright 2022 Rive
 */

#include "rive/text/line_breaker.hpp"

using namespace rive;

// Return the index for the run that contains the char at textOffset
static int _offsetToRunIndex(Span<const RenderGlyphRun> runs, size_t textOffset) {
    assert(textOffset >= 0);
    for (int i = 0; i < (int)runs.size() - 1; ++i) {
        if (textOffset <= runs[i].textOffsets.back()) {
            return i;
        }
    }
    return (int)runs.size() - 1;
}

static int textOffsetToGlyphIndex(const RenderGlyphRun& run, size_t textOffset) {
    assert(textOffset >= run.textOffsets.front());
    //    assert(textOffset <= run.textOffsets.back()); // not true for last run

    // todo: bsearch?
    auto begin = run.textOffsets.begin();
    auto end = run.textOffsets.end();
    auto iter = std::find(begin, end, textOffset);
    if (iter == end) { // end of run
        return (int)run.glyphs.size() - 1;
    }
    return (int)(iter - begin);
}

std::vector<RenderGlyphLine>
RenderGlyphLine::BreakLines(Span<const RenderGlyphRun> runs, Span<const int> breaks, float width) {
    assert(breaks.size() >= 2);

    std::vector<RenderGlyphLine> lines;
    int startRun = 0;
    int startIndex = 0;
    double xlimit = width;

    int prevRun = 0;
    int prevIndex = 0;

    int wordStart = breaks[0];
    int wordEnd = breaks[1];
    int nextBreakIndex = 2;
    int lineStartTextOffset = wordStart;

    for (;;) {
        assert(wordStart <= wordEnd); // == means trailing spaces?

        int endRun = _offsetToRunIndex(runs, wordEnd);
        int endIndex = textOffsetToGlyphIndex(runs[endRun], wordEnd);
        float pos = runs[endRun].xpos[endIndex];
        bool bumpBreakIndex = true;
        if (pos > xlimit) {
            int wsRun = _offsetToRunIndex(runs, wordStart);
            int wsIndex = textOffsetToGlyphIndex(runs[wsRun], wordStart);

            bumpBreakIndex = false;
            // does just one word not fit?
            if (lineStartTextOffset == wordStart) {
                // walk backwards a letter at a time until we fit, stopping at
                // 1 letter.
                int wend = wordEnd;
                while (pos > xlimit && wend - 1 > wordStart) {
                    wend -= 1;
                    prevRun = _offsetToRunIndex(runs, wend);
                    prevIndex = textOffsetToGlyphIndex(runs[prevRun], wend);
                    pos = runs[prevRun].xpos[prevIndex];
                }
                assert(wend < wordEnd || wend == wordEnd && wordStart + 1 == wordEnd);
                if (wend == wordEnd) {
                    bumpBreakIndex = true;
                }

                // now reset our "whitespace" marker to just be prev, since
                // by defintion we have no extra whitespace on this line
                wsRun = prevRun;
                wsIndex = prevIndex;
                wordStart = wend;
            }

            // bulid the line
            const auto lineStartX = runs[startRun].xpos[startIndex];
            lines.push_back(RenderGlyphLine(
                startRun, startIndex, prevRun, prevIndex, wsRun, wsIndex, lineStartX));

            // update for the next line
            xlimit = runs[wsRun].xpos[wsIndex] + width;
            startRun = prevRun = wsRun;
            startIndex = prevIndex = wsIndex;
            lineStartTextOffset = wordStart;
        } else {
            // we didn't go too far, so remember this word-end boundary
            prevRun = endRun;
            prevIndex = endIndex;
        }

        if (bumpBreakIndex) {
            if (nextBreakIndex < breaks.size()) {
                wordStart = breaks[nextBreakIndex++];
                wordEnd = breaks[nextBreakIndex++];
            } else {
                break; // bust out of the loop
            }
        }
    }
    // scoop up the last line (if present)
    const int tailRun = (int)runs.size() - 1;
    const int tailIndex = (int)runs[tailRun].glyphs.size();
    if (startRun != tailRun || startIndex != tailIndex) {
        const auto startX = runs[startRun].xpos[startIndex];
        lines.push_back(
            RenderGlyphLine(startRun, startIndex, tailRun, tailIndex, tailRun, tailIndex, startX));
    }

    ComputeLineSpacing(toSpan(lines), runs);

    return lines;
}

void RenderGlyphLine::ComputeLineSpacing(Span<RenderGlyphLine> lines,
                                         Span<const RenderGlyphRun> runs) {
    float Y = 0; // top of our frame
    for (auto& line : lines) {
        float asc = 0;
        float des = 0;
        for (int i = line.startRun; i <= line.wsRun; ++i) {
            const auto& run = runs[i];

            asc = std::min(asc, run.font->lineMetrics().ascent * run.size);
            des = std::max(des, run.font->lineMetrics().descent * run.size);
        }
        line.top = Y;
        Y -= asc;
        line.baseline = Y;
        Y += des;
        line.bottom = Y;
    }
    // TODO: good place to perform left/center/right alignment
}
