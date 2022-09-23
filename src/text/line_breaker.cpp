/*
 * Copyright 2022 Rive
 */

#include "rive/text/line_breaker.hpp"
#include <limits>
using namespace rive;

static bool autowidth(float width) { return width < 0.0f; }
void RenderGlyphLine::ComputeLineSpacing(Span<RenderGlyphLine> lines,
                                         Span<const RenderGlyphRun> runs,
                                         float width,
                                         RenderTextAlign align) {

    float maxLineWidth = 0.0f;
    if (autowidth(width)) {
        for (auto& line : lines) {
            auto lineWidth =
                runs[line.endRun].xpos[line.endIndex] - runs[line.startRun].xpos[line.startIndex];
            if (lineWidth > maxLineWidth) {
                maxLineWidth = lineWidth;
            }
        }
    } else {
        maxLineWidth = width;
    }
    float Y = 0; // top of our frame
    for (auto& line : lines) {
        float asc = 0;
        float des = 0;
        for (int i = line.startRun; i <= line.endRun; ++i) {
            const auto& run = runs[i];

            asc = std::min(asc, run.font->lineMetrics().ascent * run.size);
            des = std::max(des, run.font->lineMetrics().descent * run.size);
        }
        line.top = Y;
        Y -= asc;
        line.baseline = Y;
        Y += des;
        line.bottom = Y;
        auto lineWidth =
            runs[line.endRun].xpos[line.endIndex] - runs[line.startRun].xpos[line.startIndex];
        switch (align) {
            case RenderTextAlign::right: line.startX = maxLineWidth - lineWidth; break;
            case RenderTextAlign::left: line.startX = 0; break;
            case RenderTextAlign::center:
                line.startX = maxLineWidth / 2.0f - lineWidth / 2.0f;
                break;
        }
    }
}

struct WordMarker {
    const RenderGlyphRun* run;
    uint32_t index;

    bool next(Span<const RenderGlyphRun> runs) {
        index += 2;
        while (index >= run->breaks.size()) {

            index -= run->breaks.size();
            run++;
            if (run == runs.end()) {
                return false;
            }
        }
        return true;
    }
};

class RunIterator {
    Span<const RenderGlyphRun> m_runs;
    const RenderGlyphRun* m_run;
    uint32_t m_index;

public:
    RunIterator(Span<const RenderGlyphRun> runs, const RenderGlyphRun* run, uint32_t index) :
        m_runs(runs), m_run(run), m_index(index) {}

    bool back() {
        if (m_index == 0) {
            if (m_run == m_runs.begin()) {
                return false;
            }
            m_run--;
            if (m_run->glyphs.size() == 0) {
                m_index = 0;
                return back();
            } else {
                m_index = m_run->glyphs.size() == 0 ? 0 : (uint32_t)m_run->glyphs.size() - 1;
            }
        } else {
            m_index--;
        }
        return true;
    }

    bool forward() {
        if (m_index == m_run->glyphs.size()) {
            if (m_run == m_runs.end()) {
                return false;
            }
            m_run++;
            m_index = 0;
            if (m_index == m_run->glyphs.size()) {
                return forward();
            }
        } else {
            m_index++;
        }
        return true;
    }

    float x() const { return m_run->xpos[m_index]; }

    const RenderGlyphRun* run() const { return m_run; }
    uint32_t index() const { return m_index; }

    bool operator==(const RunIterator& o) const { return m_run == o.m_run && m_index == o.m_index; }
};

std::vector<RenderGlyphLine> RenderGlyphLine::BreakLines(Span<const RenderGlyphRun> runs,
                                                         float width,
                                                         RenderTextAlign align) {

    float maxLineWidth = autowidth(width) ? std::numeric_limits<float>::max() : width;

    std::vector<RenderGlyphLine> lines;

    if (runs.empty()) {
        return lines;
    }

    auto limit = maxLineWidth;

    bool advanceWord = false;
    WordMarker start = {runs.begin(), 0};
    WordMarker end = {runs.begin(), 1};

    RenderGlyphLine line = RenderGlyphLine();

    uint32_t breakIndex = end.run->breaks[end.index];
    uint32_t lastEndIndex = end.index;
    uint32_t startBreakIndex = start.run->breaks[start.index];
    float x = end.run->xpos[breakIndex];
    while (true) {
        if (advanceWord) {
            lastEndIndex = end.index;

            if (!start.next(runs)) {
                break;
            }
            if (!end.next(runs)) {
                break;
            }

            advanceWord = false;

            breakIndex = end.run->breaks[end.index];
            startBreakIndex = start.run->breaks[start.index];
            x = end.run->xpos[breakIndex];
        }

        if (breakIndex != startBreakIndex && x > limit) {
            uint32_t startRun = (uint32_t)(start.run - runs.begin());

            // A whole word overflowed, break until we can no longer break (or
            // it fits).
            if (line.startRun == startRun && line.startIndex == startBreakIndex) {
                bool canBreakMore = true;
                while (canBreakMore && x > limit) {

                    RunIterator lineStart =
                        RunIterator(runs, runs.begin() + line.startRun, line.startIndex);
                    RunIterator lineEnd = RunIterator(runs, end.run, end.run->breaks[end.index]);

                    // Look for the next character that doesn't overflow.
                    while (true) {
                        if (!lineEnd.back()) {
                            // Hit the start of the text, can't go back.
                            canBreakMore = false;
                            break;
                        } else if (lineEnd.x() <= limit) {
                            if (lineStart == lineEnd && !lineEnd.forward()) {
                                // Hit the start of the line and could not
                                // go forward to consume a single character.
                                // We can't break any further.
                                canBreakMore = false;
                            } else {
                                line.endRun = (uint32_t)(lineEnd.run() - runs.begin());
                                line.endIndex = lineEnd.index();
                            }
                            break;
                        }
                    }
                    if (canBreakMore) {
                        // Add the line and push the limit out.
                        limit = lineEnd.x() + maxLineWidth;
                        if (!line.empty()) {
                            lines.push_back(line);
                        }
                        // Setup the next line.
                        line = RenderGlyphLine((uint32_t)(lineEnd.run() - runs.begin()),
                                               lineEnd.index());
                    }
                }
            } else {
                // word overflowed, knock it to a new line
                auto startX = start.run->xpos[start.run->breaks[start.index]];
                limit = startX + maxLineWidth;

                if (!line.empty() || start.index - lastEndIndex > 1) {
                    lines.push_back(line);
                }

                line = RenderGlyphLine(startRun, startBreakIndex);
            }
        } else {
            line.endRun = (uint32_t)(end.run - runs.begin());
            line.endIndex = end.run->breaks[end.index];
            advanceWord = true;
            // Forced BR.
            if (breakIndex == startBreakIndex) {
                lines.push_back(line);
                auto startX = start.run->xpos[start.run->breaks[start.index]];
                limit = startX + maxLineWidth;
                line = RenderGlyphLine((uint32_t)(start.run - runs.begin()), startBreakIndex + 1);
            }
        }
    }
    // Don't add a line that starts/ends at the same spot.
    if (!line.empty()) {
        lines.push_back(line);
    }

    ComputeLineSpacing(lines, runs, width, align);
    return lines;
}