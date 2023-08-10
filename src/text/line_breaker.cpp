/*
 * Copyright 2022 Rive
 */

#include "rive/text_engine.hpp"
#include <limits>
#include <algorithm>
using namespace rive;

static bool autowidth(float width) { return width < 0.0f; }

float GlyphLine::ComputeMaxWidth(Span<GlyphLine> lines, Span<const GlyphRun> runs)
{
    float maxLineWidth = 0.0f;
    for (auto& line : lines)
    {
        maxLineWidth = std::max(maxLineWidth,
                                runs[line.endRunIndex].xpos[line.endGlyphIndex] -
                                    runs[line.startRunIndex].xpos[line.startGlyphIndex] -
                                    runs[line.endRunIndex].letterSpacing);
    }
    return maxLineWidth;
}

static const rive::Font::LineMetrics computeLineMetrics(const rive::Font::LineMetrics& metrics,
                                                        float customLineHeight,
                                                        float fontSize)
{
    if (customLineHeight < 0.0f)
    {
        return {metrics.ascent * fontSize, metrics.descent * fontSize};
    }
    float baseline = -metrics.ascent;
    float height = baseline + metrics.descent;
    float baselineFactor = baseline / height;

    float actualAscent = -baselineFactor * customLineHeight;

    return {actualAscent, customLineHeight + actualAscent};
}

void GlyphLine::ComputeLineSpacing(bool isFirstLine,
                                   Span<GlyphLine> lines,
                                   Span<const GlyphRun> runs,
                                   float width,
                                   TextAlign align)
{
    bool first = isFirstLine;
    float Y = 0; // top of our frame
    for (auto& line : lines)
    {
        float asc = 0;
        float realAscent = 0;
        float des = 0;
        float lh = 0;
        for (uint32_t i = line.startRunIndex; i <= line.endRunIndex; ++i)
        {
            const auto& run = runs[i];
            const auto& metrics =
                computeLineMetrics(run.font->lineMetrics(), run.lineHeight, run.size);
            realAscent = std::min(realAscent, run.font->lineMetrics().ascent * run.size);
            asc = std::min(asc, metrics.ascent);
            des = std::max(des, metrics.descent);
            if (run.lineHeight >= 0.0f)
            {
                lh = std::max(lh, run.lineHeight);
            }
            else
            {
                lh = std::max(lh, -asc + des);
            }
        }
        line.top = Y;
        if (first)
        {
            Y = -realAscent;
            first = false;
        }
        else
        {
            Y -= asc;
        }
        line.baseline = Y;
        Y += des;
        line.bottom = Y;

        auto lineWidth = runs[line.endRunIndex].xpos[line.endGlyphIndex] -
                         runs[line.startRunIndex].xpos[line.startGlyphIndex] -
                         runs[line.endRunIndex].letterSpacing;
        switch (align)
        {
            case TextAlign::right:
                line.startX = width - lineWidth;
                break;
            case TextAlign::left:
                line.startX = 0;
                break;
            case TextAlign::center:
                line.startX = width / 2.0f - lineWidth / 2.0f;
                break;
        }
    }
}

struct WordMarker
{
    const GlyphRun* run;
    uint32_t index;

    bool next(Span<const GlyphRun> runs)
    {
        index += 2;
        while (index >= run->breaks.size())
        {
            index -= run->breaks.size();
            run++;
            if (run == runs.end())
            {
                return false;
            }
        }
        return true;
    }
};

class RunIterator
{
    Span<const GlyphRun> m_runs;
    const GlyphRun* m_run;
    uint32_t m_index;

public:
    RunIterator(Span<const GlyphRun> runs, const GlyphRun* run, uint32_t index) :
        m_runs(runs), m_run(run), m_index(index)
    {}

    bool back()
    {
        if (m_index == 0)
        {
            if (m_run == m_runs.begin())
            {
                return false;
            }
            m_run--;
            if (m_run->glyphs.size() == 0)
            {
                m_index = 0;
                return back();
            }
            else
            {
                m_index = m_run->glyphs.size() == 0 ? 0 : (uint32_t)m_run->glyphs.size() - 1;
            }
        }
        else
        {
            m_index--;
        }
        return true;
    }

    bool forward()
    {
        if (m_index == m_run->glyphs.size())
        {
            if (m_run == m_runs.end())
            {
                return false;
            }
            m_run++;
            m_index = 0;
            if (m_index == m_run->glyphs.size())
            {
                return forward();
            }
        }
        else
        {
            m_index++;
        }
        return true;
    }

    float x() const { return m_run->xpos[m_index]; }

    const GlyphRun* run() const { return m_run; }
    uint32_t index() const { return m_index; }

    bool operator==(const RunIterator& o) const { return m_run == o.m_run && m_index == o.m_index; }
};

SimpleArray<GlyphLine> GlyphLine::BreakLines(Span<const GlyphRun> runs, float width)
{
    float maxLineWidth = autowidth(width) ? std::numeric_limits<float>::max() : width;

    SimpleArrayBuilder<GlyphLine> lines;

    if (runs.empty())
    {
        return lines;
    }

    auto limit = maxLineWidth;

    bool advanceWord = false;

    // We iterate the breaks list with a WordMarker helper which is basically an
    // iterator. The breaks lists contains tightly packed start/end indices per
    // run. So the first valid word is at break index 0,1 (per run). Because a
    // run can be created with no valid breaks, we start the word iterator at a
    // negative index and attempt to move it to the first valid index (which
    // could be in the Nth run in the paragraph). If that fails, we know we have
    // no words to break in the entire paragraph and can early out. See how
    // WordMarker::next works and notice how we also use it below in this same
    // method.
    WordMarker start = {runs.begin(), (uint32_t)-2};
    WordMarker end = {runs.begin(), (uint32_t)-1};
    if (!start.next(runs) || !end.next(runs))
    {
        return lines;
    }

    GlyphLine line = GlyphLine();

    uint32_t breakIndex = end.run->breaks[end.index];
    const GlyphRun* breakRun = end.run;
    uint32_t lastEndGlyphIndex = end.index;
    uint32_t startBreakIndex = start.run->breaks[start.index];
    const GlyphRun* startBreakRun = start.run;

    float x = end.run->xpos[breakIndex];
    while (true)
    {
        if (advanceWord)
        {
            lastEndGlyphIndex = end.index;

            if (!start.next(runs))
            {
                break;
            }
            if (!end.next(runs))
            {
                break;
            }

            advanceWord = false;

            breakIndex = end.run->breaks[end.index];
            breakRun = end.run;
            startBreakIndex = start.run->breaks[start.index];
            startBreakRun = start.run;
            x = end.run->xpos[breakIndex];
        }

        bool isForcedBreak = breakRun == startBreakRun && breakIndex == startBreakIndex;

        if (!isForcedBreak && x > limit)
        {
            uint32_t startRunIndex = (uint32_t)(start.run - runs.begin());

            // A whole word overflowed, break until we can no longer break (or
            // it fits).
            if (line.startRunIndex == startRunIndex && line.startGlyphIndex == startBreakIndex)
            {
                bool canBreakMore = true;
                while (canBreakMore && x > limit)
                {

                    RunIterator lineStart =
                        RunIterator(runs, runs.begin() + line.startRunIndex, line.startGlyphIndex);
                    RunIterator lineEnd = RunIterator(runs, end.run, end.run->breaks[end.index]);
                    // Look for the next character that doesn't overflow.
                    while (true)
                    {
                        if (!lineEnd.back())
                        {
                            // Hit the start of the text, can't go back.
                            canBreakMore = false;
                            break;
                        }
                        else if (lineEnd.x() <= limit)
                        {
                            if (lineStart == lineEnd && !lineEnd.forward())
                            {
                                // Hit the start of the line and could not
                                // go forward to consume a single character.
                                // We can't break any further.
                                canBreakMore = false;
                            }
                            else
                            {
                                line.endRunIndex = (uint32_t)(lineEnd.run() - runs.begin());
                                line.endGlyphIndex = lineEnd.index();
                            }
                            break;
                        }
                    }
                    if (canBreakMore)
                    {
                        // Add the line and push the limit out.
                        limit = lineEnd.x() + maxLineWidth;
                        if (!line.empty())
                        {
                            lines.add(line);
                        }
                        // Setup the next line.
                        line = GlyphLine((uint32_t)(lineEnd.run() - runs.begin()), lineEnd.index());
                    }
                }
            }
            else
            {
                // word overflowed, knock it to a new line
                auto startX = start.run->xpos[start.run->breaks[start.index]];
                limit = startX + maxLineWidth;

                if (!line.empty() || start.index - lastEndGlyphIndex > 1)
                {
                    lines.add(line);
                }

                line = GlyphLine(startRunIndex, startBreakIndex);
            }
        }
        else
        {
            line.endRunIndex = (uint32_t)(end.run - runs.begin());
            line.endGlyphIndex = end.run->breaks[end.index];
            advanceWord = true;
            // Forced BR.
            if (isForcedBreak)
            {
                lines.add(line);
                auto startX = start.run->xpos[start.run->breaks[start.index] + 1];
                limit = startX + maxLineWidth;
                line = GlyphLine((uint32_t)(start.run - runs.begin()), startBreakIndex + 1);
            }
        }
    }

    // Add the last line.
    if (!line.empty())
    {
        lines.add(line);
    }

    return lines;
}
