#include "rive/text/cursor.hpp"
#ifdef WITH_RIVE_TEXT
using namespace rive;

static uint32_t absDiffUint(uint32_t a, uint32_t b)
{
    return a > b ? a - b : b - a;
}

static uint32_t subtractUint32(uint32_t a, uint32_t b)
{
    return a > b ? a - b : 0;
}

CursorVisualPosition CursorPosition::visualPosition(
    const FullyShapedText& shape) const
{
    const GlyphLookup& glyphLookup = shape.glyphLookup();
    const std::vector<OrderedLine>& orderedLines = shape.orderedLines();

    uint32_t targetIndex = glyphLookup[m_codePointIndex];
    if (m_lineIndex < 0 || m_lineIndex >= orderedLines.size())
    {
        return CursorVisualPosition::missing();
    }
    const OrderedLine& orderedLine = orderedLines[m_lineIndex];

    const GlyphLine& line = orderedLine.glyphLine();
    float x = line.startX;

    // Iterate each glyph and see if it's the one we're looking for.
    bool haveFirstIndex = false;
    uint32_t firstTextIndex = 0, lastTextIndex = 0;
    for (auto glyphItr : orderedLine)
    {
        const GlyphRun* run = std::get<0>(glyphItr);
        size_t glyphIndex = std::get<1>(glyphItr);
        float advance = run->advances[glyphIndex];

        if (advance != 0 &&
            targetIndex == glyphLookup[run->textIndices[glyphIndex]])
        {
            x += advance *
                 glyphLookup.advanceFactor(m_codePointIndex,
                                           run->dir() == TextDirection::rtl);

            const Font* font = run->font.get();
            return CursorVisualPosition(
                x,
                orderedLine.y() + font->ascent(run->size),
                orderedLine.y() + font->descent(run->size));
        }
        else
        {
            if (!haveFirstIndex)
            {
                firstTextIndex = lastTextIndex = run->textIndices[glyphIndex];
                haveFirstIndex = true;
            }
            else
            {
                lastTextIndex = run->textIndices[glyphIndex];
            }
            x += advance;
        }
    }

    // Didn't find the glyph, we're at the end of the line.
    const GlyphRun* run = orderedLine.lastRun();
    const Font* font = run->font.get();

    return CursorVisualPosition(
        absDiffUint(m_codePointIndex, firstTextIndex) <
                absDiffUint(m_codePointIndex, lastTextIndex)
            ? line.startX
            : x,
        orderedLine.y() + font->ascent(run->size),
        orderedLine.y() + font->descent(run->size));
}

CursorPosition CursorPosition::fromTranslation(const Vec2D translation,
                                               const FullyShapedText& shape)
{
    const std::vector<OrderedLine>& orderedLines = shape.orderedLines();
    if (orderedLines.empty())
    {
        return CursorPosition::zero();
    }

    uint32_t lineIndex = 0;
    uint32_t maxLine = (uint32_t)(orderedLines.size() - 1);
    for (const OrderedLine& orderedLine : orderedLines)
    {
        if (orderedLine.bottom() < translation.y && lineIndex != maxLine)
        {
            lineIndex++;
            continue;
        }
        return fromOrderedLine(orderedLine, lineIndex, translation.x, shape);
    }

    return CursorPosition::zero();
}

CursorPosition CursorPosition::fromOrderedLine(const OrderedLine& orderedLine,
                                               uint32_t lineIndex,
                                               float translationX,
                                               const FullyShapedText& shape)
{
    const GlyphLookup& glyphLookup = shape.glyphLookup();

    float x = orderedLine.glyphLine().startX;
    /// Iterate each glyph and see if it's the one we're looking for.
    auto end = orderedLine.end();
    GlyphItr lastGlyphItr = orderedLine.begin();
    for (GlyphItr itr = orderedLine.begin(); itr != end; ++itr)
    {
        lastGlyphItr = itr;
        const GlyphRun* run = itr.run();
        uint32_t glyphIndex = itr.glyphIndex();
        float advance = run->advances[glyphIndex];
        if (translationX <= x + advance)
        {
            float ratio = advance == 0.0f
                              ? 1.0f
                              : std::min((translationX - x) / advance, 1.0f);
            uint32_t textIndex = run->textIndices[glyphIndex];
            uint32_t nextTextIndex = textIndex;
            uint32_t absoluteGlyphIndex = glyphLookup[textIndex];
            while (nextTextIndex !=
                       subtractUint32((uint32_t)glyphLookup.size(), 1) &&
                   glyphLookup[nextTextIndex] == absoluteGlyphIndex)
            {
                nextTextIndex++;
            }
            uint32_t parts = nextTextIndex - textIndex;
            uint32_t part = (uint32_t)std::round(ratio * (float)parts);

            return CursorPosition(
                       lineIndex,
                       run->dir() == TextDirection::ltr
                           ? textIndex + part
                           : (part > nextTextIndex ? 0 : nextTextIndex - part))
                .clamped(shape);
        }
        else
        {
            x += advance;
        }
    }

    const GlyphRun* run = lastGlyphItr.run();
    uint32_t glyphIndex = lastGlyphItr.glyphIndex();

    uint32_t textIndex = run->textIndices[glyphIndex];
    uint32_t nextTextIndex = textIndex;
    uint32_t absoluteGlyphIndex = glyphLookup[textIndex];
    while (nextTextIndex != subtractUint32((uint32_t)glyphLookup.size(), 1) &&
           glyphLookup[nextTextIndex] == absoluteGlyphIndex)
    {
        nextTextIndex++;
    }
    uint32_t parts = nextTextIndex - textIndex;
    return CursorPosition(lineIndex,
                          run->dir() == TextDirection::ltr
                              ? textIndex + parts
                              : nextTextIndex - parts)
        .clamped(shape);
}

CursorPosition CursorPosition::fromLineX(uint32_t lineIndex,
                                         float x,
                                         const FullyShapedText& shape)
{
    const std::vector<OrderedLine>& orderedLines = shape.orderedLines();
    if (lineIndex >= orderedLines.size())
    {
        return CursorPosition::zero();
    }

    return fromOrderedLine(orderedLines[lineIndex], lineIndex, x, shape);
}

uint32_t CursorPosition::lineIndex(int32_t inc) const
{
    if (inc < 0 && (uint32_t)(-inc) > m_lineIndex)
    {
        return 0;
    }
    return m_lineIndex + inc;
}

uint32_t CursorPosition::codePointIndex(int32_t inc) const
{
    if (inc < 0 && (uint32_t)(-inc) > m_codePointIndex)
    {
        return 0;
    }
    return m_codePointIndex + inc;
}

CursorPosition CursorPosition::clamped(const FullyShapedText& shape) const
{
    return CursorPosition(
        std::min(m_lineIndex,
                 subtractUint32((uint32_t)shape.orderedLines().size(), 1)),
        std::min(m_codePointIndex,
                 subtractUint32(shape.glyphLookup().lastCodeUnitIndex(), 1)));
}

CursorPosition CursorPosition::atIndex(uint32_t codePointIndex,
                                       const FullyShapedText& shape)
{
    // Don't go to actual last code unit index as we always insert a zero width
    // space.
    // https://en.wikipedia.org/wiki/Zero-width_space
    if (codePointIndex >=
        subtractUint32(shape.glyphLookup().lastCodeUnitIndex(), 1))
    {
        return CursorPosition(
            subtractUint32((uint32_t)shape.orderedLines().size(), 1),
            subtractUint32(shape.glyphLookup().lastCodeUnitIndex(), 1));
    }

    const SimpleArray<Paragraph>& paragraphs = shape.paragraphs();
    const SimpleArray<SimpleArray<GlyphLine>>& paragraphLines =
        shape.paragraphLines();

    uint32_t paragraphIndex = 0;
    uint32_t lineIndex = 0;
    for (const SimpleArray<GlyphLine>& lines : paragraphLines)
    {
        const Paragraph& paragraph = paragraphs[paragraphIndex++];
        for (const GlyphLine& line : lines)
        {
            const GlyphRun& run = paragraph.runs[line.startRunIndex];
            uint32_t smallestTextIndexInLine =
                run.textIndices[line.startGlyphIndex];

            if (smallestTextIndexInLine <= codePointIndex)
            {
                lineIndex++;
                continue;
            }
            return CursorPosition(lineIndex - 1, codePointIndex).clamped(shape);
        }
    }

    return CursorPosition(lineIndex - 1, codePointIndex).clamped(shape);
}

void Cursor::selectionRects(std::vector<AABB>& rects,
                            const FullyShapedText& shape) const
{
    auto firstPosition = first().clamped(shape);
    auto lastPosition = last().clamped(shape);

    uint32_t firstLine = firstPosition.lineIndex();
    uint32_t lastLine = lastPosition.lineIndex();
    uint32_t firstCodePointIndex = firstPosition.codePointIndex();
    uint32_t lastCodePointIndex = lastPosition.codePointIndex();

    const GlyphLookup& glyphLookup = shape.glyphLookup();
    const std::vector<OrderedLine>& orderedLines = shape.orderedLines();
    for (uint32_t lineIndex = firstLine; lineIndex <= lastLine; lineIndex++)
    {
        const OrderedLine& orderedLine = orderedLines[lineIndex];
        const GlyphLine& glyphLine = orderedLine.glyphLine();
        float x = glyphLine.startX;
        for (auto glyphItr : orderedLine)
        {
            const GlyphRun* run = std::get<0>(glyphItr);
            size_t glyphIndex = std::get<1>(glyphItr);
            float advance = run->advances[glyphIndex];
            uint32_t codePointIndex = run->textIndices[glyphIndex];
            uint32_t count = glyphLookup.count(codePointIndex);
            uint32_t endCodePointIndex = codePointIndex + count;
            float y = orderedLine.y();

            // Check if this part of this glyph overlaps selection.
            if (lastCodePointIndex > codePointIndex &&
                endCodePointIndex > firstCodePointIndex)
            {
                uint32_t after =
                    subtractUint32(firstCodePointIndex, codePointIndex);
                uint32_t before =
                    subtractUint32(endCodePointIndex, lastCodePointIndex);
                float startFactor = (float)after / (float)count;
                float endFactor = (float)(count - before) / (float)count;
                if (run->dir() == TextDirection::rtl)
                {
                    startFactor = 1.0f - startFactor;
                    endFactor = 1.0f - endFactor;
                }

                auto font = run->font;
                float left = x + advance * startFactor;
                float right = x + advance * endFactor;
                if (left > right)
                {
                    float swap = right;
                    right = left;
                    left = swap;
                }
                rects.push_back(AABB(left,
                                     y + font->ascent(run->size),
                                     right,
                                     y + font->descent(run->size)));
            }
            x += advance;
        }
    }
}

void Cursor::updateSelectionPath(ShapePaintPath& path,
                                 const std::vector<AABB>& rects,
                                 const FullyShapedText& shape) const
{}

bool Cursor::resolveLinePositions(const FullyShapedText& shape)
{
    bool resolved = false;
    if (!m_start.hasLineIndex())
    {
        m_start.resolveLine(shape);
        resolved = true;
    }
    if (!m_end.hasLineIndex())
    {
        m_end.resolveLine(shape);
        resolved = true;
    }
    return resolved;
}

void CursorPosition::resolveLine(const FullyShapedText& shape)
{
    const GlyphLookup& glyphLookup = shape.glyphLookup();
    const std::vector<OrderedLine>& orderedLines = shape.orderedLines();

    uint32_t lineIndex = 0;
    for (const OrderedLine& orderedLine : orderedLines)
    {
        if (orderedLine.containsCodePointIndex(glyphLookup, m_codePointIndex))
        {
            break;
        }
        lineIndex++;
    }
    m_lineIndex = lineIndex;
}

bool Cursor::contains(uint32_t codePointIndex) const
{
    return codePointIndex >= first().codePointIndex() &&
           codePointIndex < last().codePointIndex();
}
#endif