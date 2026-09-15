#include "rive/text_engine.hpp"
#include "rive/text/utf.hpp"
#include "rive/text/glyph_lookup.hpp"
#include "rive/text/line_break.hpp"
using namespace rive;

bool rive::isWhiteSpace(Unichar c)
{
    // Zs spaces, line and paragraph separators, NEL and zero width space.
    // The no-break spaces U+00A0, U+2007 and U+202F are deliberately not.
    return c <= ' ' || c == 0x0085 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200B && c != 0x2007) || c == 0x2028 ||
           c == 0x2029 || c == 0x205F || c == 0x3000;
}

#ifdef WITH_RIVE_TEXT

static void appendUnicode(std::vector<rive::Unichar>& unichars,
                          const char text[])
{
    const uint8_t* ptr = (const uint8_t*)text;
    while (*ptr)
    {
        unichars.push_back(rive::UTF::NextUTF8(&ptr));
    }
}

static void reverseRuns(const GlyphRun** runs, int count)
{
    int halfCount = count / 2;
    int finalIndex = count - 1;

    for (int index = 0; index < halfCount; index++)
    {
        int tieIndex = finalIndex - index;

        const GlyphRun* tempRun = runs[index];
        runs[index] = runs[tieIndex];
        runs[tieIndex] = tempRun;
    }
}

OrderedLine::OrderedLine(const Paragraph& paragraph,
                         const GlyphLine& line,
                         float lineWidth,
                         bool wantEllipsis,
                         bool isEllipsisLineLast,
                         GlyphRun* ellipsisRun,
                         float y) :
    m_startGlyphIndex(line.startGlyphIndex),
    m_endGlyphIndex(line.endGlyphIndex),
    m_glyphLine(&line),
    m_y(y)
{
    std::vector<const GlyphRun*> logicalRuns;
    const SimpleArray<GlyphRun>& glyphRuns = paragraph.runs;

    if (!wantEllipsis || !buildEllipsisRuns(logicalRuns,
                                            paragraph,
                                            line,
                                            lineWidth,
                                            isEllipsisLineLast,
                                            ellipsisRun))
    {
        for (uint32_t i = line.startRunIndex; i < line.endRunIndex + 1; i++)
        {
            logicalRuns.push_back(&glyphRuns[i]);
        }

        if (!logicalRuns.empty())
        {
            m_startLogical = logicalRuns.front();
            m_endLogical = logicalRuns.back();
        }
    }

    uint8_t maxLevel = 0;
    for (auto run : logicalRuns)
    {
        if (run->level > maxLevel)
        {
            maxLevel = run->level;
        }
    }
    for (uint8_t newLevel = maxLevel; newLevel > 0; newLevel--)
    {
        for (int start = (int)(logicalRuns.size()) - 1; start >= 0; start--)
        {
            if (logicalRuns[start]->level >= newLevel)
            {
                int count = 1;
                for (; start > 0 && logicalRuns[start - 1]->level >= newLevel;
                     start--)
                {
                    count++;
                }
                reverseRuns(logicalRuns.data() + start, count);
            }
        }
    }

    m_runs = std::move(logicalRuns);
}

uint32_t OrderedLine::firstCodePointIndex(const GlyphLookup& glyphLookup) const
{
    GlyphItr index = begin();
    auto glyphIndex = index.glyphIndex();
    auto run = index.run();
    uint32_t firstCodePointIndex = run->textIndices[glyphIndex];
    if (run->dir() == TextDirection::rtl)
    {
        // If the final run is RTL we want to add the length of the final glyph
        // to get the actual last available codepoint in the line.
        firstCodePointIndex += glyphLookup.count(run->textIndices[glyphIndex]);
    }
    // Clamp ignoring last zero width space for editing.
    return std::min(firstCodePointIndex, glyphLookup.lastCodePointIndex() - 1);
}

uint32_t OrderedLine::lastCodePointIndex(const GlyphLookup& glyphLookup) const
{
    GlyphItr index = begin();
    GlyphItr lastIndex = index;

    while (index != end())
    {
        lastIndex = index;
        ++index;
    }

    auto glyphIndex = lastIndex.glyphIndex();
    auto run = lastIndex.run();

    uint32_t lastCodePointIndex = run->textIndices[glyphIndex];
    if (run->dir() == TextDirection::ltr)
    {
        lastCodePointIndex += glyphLookup.count(run->textIndices[glyphIndex]);
    }
    // Clamp ignoring last zero width space for editing.
    return std::min(lastCodePointIndex, glyphLookup.lastCodePointIndex() - 1);
}

bool OrderedLine::containsCodePointIndex(const GlyphLookup& glyphLookup,
                                         uint32_t codePointIndex) const
{
    return codePointIndex >= firstCodePointIndex(glyphLookup) &&
           codePointIndex <= lastCodePointIndex(glyphLookup);
}

void GlyphItr::tryAdvanceRun()
{
    while (true)
    {
        auto run = *m_run;
        if (m_glyphIndex == m_line->endGlyphIndex(run) &&
            run != m_line->lastRun())
        {
            m_run++;
            m_glyphIndex = m_line->startGlyphIndex(*m_run);
        }
        else
        {
            break;
        }
    }
}
GlyphItr& GlyphItr::operator++()
{
    auto run = *m_run;
    m_glyphIndex += run->dir() == TextDirection::ltr ? 1 : -1;
    tryAdvanceRun();
    return *this;
}

bool OrderedLine::buildEllipsisRuns(std::vector<const GlyphRun*>& logicalRuns,
                                    const Paragraph& paragraph,
                                    const GlyphLine& line,
                                    float lineWidth,
                                    bool isEllipsisLineLast,
                                    GlyphRun* storedEllipsisRun)
{
    float x = 0.0f;
    const SimpleArray<GlyphRun>& glyphRuns = paragraph.runs;
    uint32_t startGIndex = line.startGlyphIndex;
    // If it's the last line we can actually early out if the whole things fits,
    // so check that first with no extra shaping.
    if (isEllipsisLineLast)
    {
        bool fits = true;

        for (uint32_t i = line.startRunIndex; i < line.endRunIndex + 1; i++)
        {
            const GlyphRun& run = glyphRuns[i];
            uint32_t endGIndex = i == line.endRunIndex
                                     ? line.endGlyphIndex
                                     : (uint32_t)run.glyphs.size();

            for (uint32_t j = startGIndex; j != endGIndex; j++)
            {
                x += run.advances[j];
                if (x > lineWidth)
                {
                    fits = false;
                    goto measured;
                }
            }
            startGIndex = 0;
        }
    measured:
        if (fits)
        {
            // It fits, just get the regular glyphs.
            return false;
        }
    }

    std::vector<Unichar> ellipsisCodePoints;
    appendUnicode(ellipsisCodePoints, "...");

    rcp<Font> ellipsisFont = nullptr;
    float ellipsisFontSize = 0.0f;

    GlyphRun ellipsisRun = {};
    float ellipsisWidth = 0.0f;

    bool ellipsisOverflowed = false;
    startGIndex = line.startGlyphIndex;
    x = 0.0f;

    for (uint32_t i = line.startRunIndex; i < line.endRunIndex + 1; i++)
    {
        const GlyphRun& run = glyphRuns[i];
        if (run.font != ellipsisFont && run.size != ellipsisFontSize)
        {
            // Track the latest we've checked (even if we discard it so we don't
            // try to do this again for this ellipsis).
            ellipsisFont = run.font;
            ellipsisFontSize = run.size;

            // Get the next shape so we can check if it fits, otherwise keep
            // using the last one.
            TextRun ellipsisRuns[] = {{
                ellipsisFont,
                ellipsisFontSize,
                run.lineHeight,
                run.letterSpacing,
                (uint32_t)ellipsisCodePoints.size(),
                0, // default, TextRun.script
                run.styleId,
            }};
            auto nextEllipsisShape =
                ellipsisFont->shapeText(ellipsisCodePoints,
                                        Span<TextRun>(ellipsisRuns, 1));

            // Hard assumption one run and para
            const Paragraph& para = nextEllipsisShape[0];
            const GlyphRun& nextEllipsisRun = para.runs.front();

            float nextEllipsisWidth = 0;
            for (size_t j = 0; j < nextEllipsisRun.glyphs.size(); j++)
            {
                nextEllipsisWidth += nextEllipsisRun.advances[j];
            }

            if (ellipsisRun.font == nullptr ||
                x + nextEllipsisWidth <= lineWidth)
            {
                // This ellipsis still fits, go ahead and use it. Otherwise
                // stick with the old one.
                ellipsisWidth = nextEllipsisWidth;
                ellipsisRun = std::move(para.runs.front());
            }
        }

        uint32_t endGIndex = i == line.endRunIndex
                                 ? line.endGlyphIndex
                                 : (uint32_t)run.glyphs.size();
        for (uint32_t j = startGIndex; j != endGIndex; j++)
        {
            float advance = run.advances[j];
            if (x + advance + ellipsisWidth > lineWidth)
            {
                m_endGlyphIndex = j;
                ellipsisOverflowed = true;
                break;
            }
            x += advance;
        }
        startGIndex = 0;
        logicalRuns.push_back(&run);
        m_endLogical = &run;

        if (ellipsisOverflowed && ellipsisRun.font != nullptr)
        {
            *storedEllipsisRun = std::move(ellipsisRun);
            logicalRuns.push_back(storedEllipsisRun);
            break;
        }
    }

    // There was enough space for it, so let's add the ellipsis (if we didn't
    // already). Note that we already checked if this is the last line and found
    // that the whole text didn't fit.
    if (!ellipsisOverflowed && ellipsisRun.font != nullptr)
    {
        *storedEllipsisRun = std::move(ellipsisRun);
        logicalRuns.push_back(storedEllipsisRun);
    }
    m_startLogical = storedEllipsisRun == logicalRuns.front()
                         ? nullptr
                         : logicalRuns.front();
    return true;
}

float OrderedLine::bottom() const
{
    return m_y - m_glyphLine->baseline + m_glyphLine->bottom;
}

SimpleArray<Paragraph> Font::shapeText(Span<const Unichar> text,
                                       Span<const TextRun> runs,
                                       int textDirectionFlag) const
{
#ifdef DEBUG
    size_t count = 0;
    for (const TextRun& tr : runs)
    {
        assert(tr.unicharCount > 0);
        count += tr.unicharCount;
    }
    assert(count <= text.size());
#endif

    SimpleArray<Paragraph> paragraphs =
        onShapeText(text, runs, textDirectionFlag);
    // Boundaries live on the stack for typical UI strings.
    constexpr size_t kInlineBreaks = 257;
    LineBreak inlineBreaks[kInlineBreaks];
    std::vector<LineBreak> heapBreaks;
    size_t breakCount = text.size() + 1;
    if (breakCount > kInlineBreaks)
    {
        heapBreaks.resize(breakCount);
    }
    Span<LineBreak> lineBreaks(heapBreaks.empty() ? inlineBreaks
                                                  : heapBreaks.data(),
                               breakCount);
    computeLineBreaks(text, lineBreaks);
    bool inWord = false;
    GlyphRun* lastRun = nullptr;
    size_t reserveSize = text.size() / 4;
    SimpleArrayBuilder<uint32_t> breakBuilder(reserveSize);
    SimpleArrayBuilder<uint32_t> joinerBuilder(reserveSize);
    for (const Paragraph& para : paragraphs)
    {
        for (GlyphRun& gr : para.runs)
        {
            if (lastRun != nullptr)
            {
                lastRun->breaks = std::move(breakBuilder);
                lastRun->joiners = std::move(joinerBuilder);
                // Reset the builder.
                breakBuilder = SimpleArrayBuilder<uint32_t>(reserveSize);
                joinerBuilder = SimpleArrayBuilder<uint32_t>(reserveSize);
            }
            uint32_t glyphIndex = 0;
            uint32_t lastOffset = (uint32_t)-1;
            for (uint32_t offset : gr.textIndices)
            {
                // Only the first glyph of a cluster can start or end a word.
                if (offset == lastOffset)
                {
                    glyphIndex++;
                    continue;
                }
                lastOffset = offset;
                Unichar unicode = text[offset];
                if (lineBreaks[offset + 1] == LineBreak::mandatory)
                {
                    breakBuilder.add(glyphIndex);
                    breakBuilder.add(glyphIndex);
                }
                // Only U+2060 and U+FEFF are word joiners, skip the lookup
                // for everything below them.
                if (unicode >= 0x2060 &&
                    lineBreakProps(unicode).cls == LineBreakClass::WJ)
                {
                    joinerBuilder.add(offset);
                }
                if (inWord)
                {
                    if (isWhiteSpace(unicode))
                    {
                        breakBuilder.add(glyphIndex);
                        inWord = false;
                    }
                    // Soft hyphens stay unbreakable until we can draw the
                    // hyphen at the line end.
                    else if (lineBreaks[offset] == LineBreak::allowed &&
                             text[offset - 1] != 0x00AD)
                    {
                        breakBuilder.add(glyphIndex);
                        breakBuilder.add(glyphIndex);
                    }
                }
                else if (!isWhiteSpace(unicode))
                {
                    breakBuilder.add(glyphIndex);
                    inWord = true;
                }
                glyphIndex++;
            }

            lastRun = &gr;
        }
    }
    if (lastRun != nullptr)
    {
        if (inWord)
        {
            breakBuilder.add((uint32_t)lastRun->glyphs.size());
        }
        else
        {
            // Consume the rest of the run.
            breakBuilder.add(breakBuilder.empty() ? 0 : breakBuilder.back());
            breakBuilder.add((uint32_t)lastRun->glyphs.size());
        }
        lastRun->breaks = std::move(breakBuilder);
        lastRun->joiners = std::move(joinerBuilder);
    }

#ifdef DEBUG
    for (const Paragraph& para : paragraphs)
    {
        for (const GlyphRun& gr : para.runs)
        {
            assert(gr.glyphs.size() > 0);
            assert(gr.glyphs.size() == gr.textIndices.size());
            assert(gr.glyphs.size() + 1 == gr.xpos.size());
        }
    }
#endif
    return paragraphs;
}
#endif
