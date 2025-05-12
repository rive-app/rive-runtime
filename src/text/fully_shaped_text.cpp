#ifdef WITH_RIVE_TEXT
#include "rive/text/fully_shaped_text.hpp"
#include "rive/text/text.hpp"

using namespace rive;

void FullyShapedText::shape(Span<Unichar> text,
                            Span<TextRun> runs,
                            TextSizing sizing,
                            float maxWidth,
                            float maxHeight,
                            TextAlign alignment,
                            TextWrap wrap,
                            TextOrigin origin,
                            TextOverflow overflow,
                            float paragraphSpacing)
{
    m_paragraphs = runs[0].font->shapeText(text, runs);
    m_glyphLookup.compute(text, m_paragraphs);

    m_paragraphLines =
        Text::BreakLines(m_paragraphs,
                         sizing == TextSizing::autoWidth ? -1.0f : maxWidth,
                         alignment,
                         wrap);
    m_orderedLines.clear();
    m_ellipsisRun = {};

    // build render styles.
    if (m_paragraphs.empty())
    {
        m_bounds = AABB(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    // Build up ordered runs as we go.
    int32_t paragraphIndex = 0;
    float y = 0.0f;
    float minY = 0.0f;
    float measuredWidth = 0.0f;
    if (origin == TextOrigin::baseline && !m_paragraphLines.empty() &&
        !m_paragraphLines[0].empty())
    {
        y -= m_paragraphLines[0][0].baseline;
        minY = y;
    }

    int ellipsisLine = -1;
    bool isEllipsisLineLast = false;
    // Find the line to put the ellipsis on (line before the one that
    // overflows).
    bool wantEllipsis =
        overflow == TextOverflow::ellipsis && sizing == TextSizing::fixed;

    int lastLineIndex = -1;
    for (const SimpleArray<GlyphLine>& paragraphLines : m_paragraphLines)
    {
        const Paragraph& paragraph = m_paragraphs[paragraphIndex++];
        for (const GlyphLine& line : paragraphLines)
        {
            const GlyphRun& endRun = paragraph.runs[line.endRunIndex];
            const GlyphRun& startRun = paragraph.runs[line.startRunIndex];
            float width = endRun.xpos[line.endGlyphIndex] -
                          startRun.xpos[line.startGlyphIndex];
            if (width > measuredWidth)
            {
                measuredWidth = width;
            }
            lastLineIndex++;
            if (wantEllipsis && y + line.bottom <= maxHeight)
            {
                ellipsisLine++;
            }
        }

        if (!paragraphLines.empty())
        {
            y += paragraphLines.back().bottom;
        }
        y += paragraphSpacing;
    }
    if (wantEllipsis && ellipsisLine == -1)
    {
        // Nothing fits, just show the first line and ellipse it.
        ellipsisLine = 0;
    }
    isEllipsisLineLast = lastLineIndex == ellipsisLine;

    int32_t lineIndex = 0;
    paragraphIndex = 0;
    m_bounds =
        AABB(0.0f, minY, measuredWidth, std::max(minY, y - paragraphSpacing));

    y = 0;
    if (origin == TextOrigin::baseline && !m_paragraphLines.empty() &&
        !m_paragraphLines[0].empty())
    {
        y -= m_paragraphLines[0][0].baseline;
    }
    paragraphIndex = 0;

    for (const SimpleArray<GlyphLine>& paragraphLines : m_paragraphLines)
    {
        const Paragraph& paragraph = m_paragraphs[paragraphIndex++];
        for (const GlyphLine& line : paragraphLines)
        {
            switch (overflow)
            {
                case TextOverflow::hidden:
                    if (sizing == TextSizing::fixed &&
                        y + line.bottom > maxHeight)
                    {
                        return;
                    }
                    break;
                case TextOverflow::clipped:
                    if (sizing == TextSizing::fixed && y + line.top > maxHeight)
                    {
                        return;
                    }
                    break;
                default:
                    break;
            }

            m_orderedLines.emplace_back(OrderedLine(paragraph,
                                                    line,
                                                    maxWidth,
                                                    ellipsisLine == lineIndex,
                                                    isEllipsisLineLast,
                                                    &m_ellipsisRun,
                                                    y + line.baseline));

            if (lineIndex == ellipsisLine)
            {
                return;
            }
            lineIndex++;
        }
        if (!paragraphLines.empty())
        {
            y += paragraphLines.back().bottom;
        }
        y += paragraphSpacing;
    }
    return;
}
#endif