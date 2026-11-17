/*
 * Copyright 2022 Rive
 */

#include "rive/text_engine.hpp"
#include <limits>
#include <algorithm>
using namespace rive;

static bool autowidth(float width) { return width < 0.0f; }

float GlyphLine::ComputeMaxWidth(
    Span<GlyphLine> lines,
    Span<const GlyphRun> runs)
{
  float maxLineWidth = 0.0f;
  for (auto& line : lines)
  {
    maxLineWidth = std::max(
        maxLineWidth,
        runs[line.endRunIndex].xpos[line.endGlyphIndex] -
            runs[line.startRunIndex].xpos[line.startGlyphIndex]);
  }
  return maxLineWidth;
}

static const rive::Font::LineMetrics computeLineMetrics(
    const rive::Font::LineMetrics& metrics,
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

void GlyphLine::ComputeLineSpacing(
    bool isFirstLine,
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
      const auto& metrics = computeLineMetrics(
          run.font->lineMetrics(),
          run.lineHeight,
          run.size);
      realAscent = std::min(
          realAscent,
          run.font->lineMetrics().ascent * run.size);
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
                     runs[line.startRunIndex].xpos[line.startGlyphIndex];
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
  RunIterator(Span<const GlyphRun> runs, const GlyphRun* run, uint32_t index)
      : m_runs(runs)
      , m_run(run)
      , m_index(index)
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
        m_index = m_run->glyphs.size() == 0
                      ? 0
                      : (uint32_t)m_run->glyphs.size() - 1;
      }
    }
    else
    {
      m_index--;
    }
    // Never stop inside a glyph cluster.
    while (m_index > 0 &&
           m_run->textIndices[m_index] == m_run->textIndices[m_index - 1])
    {
      m_index--;
    }
    // If there are any word-joiners, do not break on them and find the
    // closest index that does not wrap a WJ
    if (m_run->joiners.size() > 0 && m_run->textIndices[m_index] > 0)
    {
      auto joiners = m_run->joiners;
      auto wordJoinerIndex = m_run->textIndices[m_index] - 1;
      size_t mid = 0;
      size_t start = 0;
      size_t end = joiners.size();
      // If this condition is not met, skip to avoid binary search
      if (wordJoinerIndex >= joiners[start] &&
          wordJoinerIndex < joiners[end - 1])
      {
        while (start < end)
        {
          mid = (start + end) >> 1;
          auto joinerCandidate = joiners[mid];
          if (joinerCandidate == wordJoinerIndex)
          {
            // we found that the previous text index is a joiner
            // now let's clean all indexes that are pointing to word
            // joiners
            bool isJoiner = true;
            auto currentJoinerIndex = mid;
            while (isJoiner)
            {
              if (m_index == 0)
              {
                return back();
              }
              // If the text index of the previous glyph is a
              // joiner, we need to skip it
              if (m_run->textIndices[m_index - 1] ==
                  joiners[currentJoinerIndex])
              {
                m_index--;
                if (currentJoinerIndex > 0)
                {
                  // we keep moving up the joiners list in
                  // case there are consecutive word joiners
                  currentJoinerIndex--;
                }
                else
                {
                  isJoiner = false;
                }
              }
              else
              {
                isJoiner = false;
              }
            }
            return back();
          }
          if (joinerCandidate < wordJoinerIndex)
          {
            start = mid + 1;
          }
          else
          {
            end = mid;
          }
        }
      }
    }
    return true;
  }

  bool forward()
  {
    if (m_index == m_run->glyphs.size())
    {
      if (m_run + 1 == m_runs.end())
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
    while (m_index > 0 && m_index < m_run->glyphs.size() &&
           m_run->textIndices[m_index] == m_run->textIndices[m_index - 1])
    {
      m_index++;
    }
    if (m_run->joiners.size() > 0 && m_index < m_run->textIndices.size())
    {
      auto joiners = m_run->joiners;
      auto wordJoinerIndex = m_run->textIndices[m_index];
      size_t mid = 0;
      size_t start = 0;
      size_t end = joiners.size();
      // If this condition is not met, skip to avoid binary search
      if (wordJoinerIndex >= joiners[start] &&
          wordJoinerIndex < joiners[end - 1])
      {

        while (start < end)
        {
          mid = (start + end) >> 1;
          auto joinerCandidate = joiners[mid];
          if (joinerCandidate == wordJoinerIndex)
          {
            // we found that the next text index is a joiner
            // now let's clean all indexes that are pointing to word
            // joiners
            bool isJoiner = true;
            auto currentJoinerIndex = mid;
            while (isJoiner)
            {
              if (m_index == m_run->glyphs.size())
              {
                return forward();
              }
              // If the text index of the current glyph is a
              // joiner, we need to skip it
              if (currentJoinerIndex < joiners.size() &&
                  m_run->textIndices[m_index] == joiners[currentJoinerIndex])
              {
                m_index++;
                if (currentJoinerIndex < joiners.size())
                {
                  // we keep moving down the joiners list in
                  // case there are consecutive word joiners
                  currentJoinerIndex++;
                }
                else
                {
                  isJoiner = false;
                }
              }
              else
              {
                isJoiner = false;
              }
            }
            return forward();
          }
          if (joinerCandidate < wordJoinerIndex)
          {
            start = mid + 1;
          }
          else
          {
            end = mid;
          }
        }
      }
    }
    return true;
  }

  float x() const { return m_run->xpos[m_index]; }

  const GlyphRun* run() const { return m_run; }
  uint32_t index() const { return m_index; }

  bool operator==(const RunIterator& o) const
  {
    return m_run == o.m_run && m_index == o.m_index;
  }

  bool operator<(const RunIterator& o) const
  {
    return m_run < o.m_run || (m_run == o.m_run && m_index < o.m_index);
  }
};

// Ends the line at the word's end, or where it starts when a step over word
// joiners carried its start past that.
static void endLine(GlyphLine& line, uint32_t runIndex, uint32_t glyphIndex)
{
  bool behindStart = runIndex < line.startRunIndex ||
                     (runIndex == line.startRunIndex &&
                      glyphIndex < line.startGlyphIndex);
  line.endRunIndex = behindStart ? line.startRunIndex : runIndex;
  line.endGlyphIndex = behindStart ? line.startGlyphIndex : glyphIndex;
}

// Whether the last line added holds a glyph, as it does once a step over word
// joiners carries its end past that glyph.
static bool placed(
    const SimpleArrayBuilder<GlyphLine>& lines,
    uint32_t runIndex,
    uint32_t glyphIndex)
{
  if (lines.empty())
  {
    return false;
  }
  const GlyphLine& last = lines.back();
  return runIndex < last.endRunIndex ||
         (runIndex == last.endRunIndex && glyphIndex < last.endGlyphIndex);
}

#ifdef TESTING
std::atomic<uint64_t> GlyphLine::sm_breakCount{0};
#endif

SimpleArray<GlyphLine> GlyphLine::BreakLines(
    Span<const GlyphRun> runs,
    float width,
    TextWordBreak wordBreak)
{
#ifdef TESTING
  sm_breakCount++;
#endif
  float maxLineWidth = autowidth(width) ? std::numeric_limits<float>::max()
                                        : width;

  SimpleArrayBuilder<GlyphLine> lines;

  if (runs.empty())
  {
    return lines;
  }

  auto limit = maxLineWidth;

  bool advanceWord = false;

  // We iterate the breaks list with a WordMarker helper which is
  // basically an iterator. The breaks lists contains tightly packed
  // start/end indices per run. So the first valid word is at break index
  // 0,1 (per run). Because a run can be created with no valid breaks, we
  // start the word iterator at a negative index and attempt to move it to
  // the first valid index (which could be in the Nth run in the
  // paragraph). If that fails, we know we have no words to break in the
  // entire paragraph and can early out. See how WordMarker::next works
  // and notice how we also use it below in this same method.
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

    bool isForcedBreak = breakRun == startBreakRun &&
                         breakIndex == startBreakIndex;

    uint32_t startRunIndex = (uint32_t)(start.run - runs.begin());
    // With its start on the line before, what is left of the word is alone
    // on this one.
    bool wordStartPlaced = placed(lines, startRunIndex, startBreakIndex);
    // Nothing has been placed on this line yet and it begins exactly at
    // this word, so the word is alone on its line and can't be knocked
    // any further down.
    bool wordStartsLine = wordStartPlaced ||
                          (line.startGlyphIndex == startBreakIndex &&
                           line.startRunIndex == startRunIndex);

    // TextWordBreak::normal never cuts a word: one that doesn't fit even
    // when alone on its line overflows the box instead, so treat it as if
    // it fit. Falling into the knock-to-a-new-line branch below would
    // spin forever -- it would rebuild an identical (empty) line and
    // never set advanceWord.
    bool overflows = !isForcedBreak && x > limit &&
                     !(wordBreak == TextWordBreak::normal && wordStartsLine);

    if (overflows)
    {
      // A whole word overflowed, break until we can no longer break
      // (or it fits). breakAll also cuts inside a word that isn't
      // alone on its line, but only once at least one cluster of it
      // still fits here. Without that bound the walk-back below -- which
      // has no lower bound, since the lineStart == lineEnd test is an
      // exact match -- could land in the whitespace *before* the word,
      // leaving the space on the end of this line (inflating the width
      // ComputeMaxWidth/ComputeLineSpacing derive from it) or at the
      // start of the next one. When not even one cluster fits we fall
      // through to knocking the whole word down, where wordStartsLine
      // becomes true and it gets cut normally.
      bool splitInsideWord = wordStartsLine;
      if (!splitInsideWord && wordBreak == TextWordBreak::breakAll)
      {
        RunIterator firstCluster(runs, start.run, startBreakIndex);
        splitInsideWord = firstCluster.forward() && firstCluster.x() <= limit;
      }
      if (splitInsideWord)
      {
        bool canBreakMore = true;
        while (canBreakMore && x > limit)
        {

          RunIterator lineStart = RunIterator(
              runs,
              runs.begin() + line.startRunIndex,
              line.startGlyphIndex);
          RunIterator lineEnd = RunIterator(
              runs,
              end.run,
              end.run->breaks[end.index]);
          // Look for the next character that doesn't overflow.
          while (true)
          {
            // Stepping back over word joiners can pass the line's start,
            // even the start of the text, and no line can end there.
            if (!lineEnd.back() || lineEnd < lineStart)
            {
              lineEnd = lineStart;
            }
            else if (lineEnd.x() > limit)
            {
              continue;
            }
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
          if (canBreakMore)
          {
            // Add the line and push the limit out.
            limit = lineEnd.x() + maxLineWidth;
            if (!line.empty())
            {
              lines.add(line);
            }
            // Setup the next line.
            line = GlyphLine(
                (uint32_t)(lineEnd.run() - runs.begin()),
                lineEnd.index());
          }
        }
        if (!canBreakMore)
        {
          // Nothing left to cut, so the rest of the word overflows.
          endLine(
              line,
              (uint32_t)(end.run - runs.begin()),
              end.run->breaks[end.index]);
          advanceWord = true;
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
      endLine(
          line,
          (uint32_t)(end.run - runs.begin()),
          end.run->breaks[end.index]);
      advanceWord = true;
      // Forced BR, unless the line before this one already took it in.
      if (isForcedBreak && !wordStartPlaced)
      {
        lines.add(line);
        auto startX = start.run->xpos[start.run->breaks[start.index] + 1];
        limit = startX + maxLineWidth;
        line = GlyphLine(
            (uint32_t)(start.run - runs.begin()),
            startBreakIndex + 1);
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
