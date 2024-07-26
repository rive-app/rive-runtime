#ifdef WITH_RIVE_TEXT
#include "rive/text/raw_text.hpp"
#include "rive/text_engine.hpp"
#include "rive/factory.hpp"

using namespace rive;

RawText::RawText(Factory* factory) : m_factory(factory) {}
bool RawText::empty() const { return m_styled.empty(); }

void RawText::append(const std::string& text,
                     rcp<RenderPaint> paint,
                     rcp<Font> font,
                     float size,
                     float lineHeight,
                     float letterSpacing)
{
    int styleIndex = 0;
    for (RenderStyle& style : m_styles)
    {
        if (style.paint == paint)
        {
            break;
        }
        styleIndex++;
    }
    if (styleIndex == m_styles.size())
    {
        m_styles.push_back({paint, m_factory->makeEmptyRenderPath(), true});
    }
    m_styled.append(font, size, lineHeight, letterSpacing, text, styleIndex);
    m_dirty = true;
}

void RawText::clear()
{
    m_styled.clear();
    m_dirty = true;
}

TextSizing RawText::sizing() const { return m_sizing; }

TextOverflow RawText::overflow() const { return m_overflow; }

TextAlign RawText::align() const { return m_align; }

float RawText::maxWidth() const { return m_maxWidth; }

float RawText::maxHeight() const { return m_maxHeight; }

float RawText::paragraphSpacing() const { return m_paragraphSpacing; }

void RawText::sizing(TextSizing value)
{
    if (m_sizing != value)
    {
        m_sizing = value;
        m_dirty = true;
    }
}

void RawText::overflow(TextOverflow value)
{
    if (m_overflow != value)
    {
        m_overflow = value;
        m_dirty = true;
    }
}

void RawText::align(TextAlign value)
{
    if (m_align != value)
    {
        m_align = value;
        m_dirty = true;
    }
}

void RawText::paragraphSpacing(float value)
{
    if (m_paragraphSpacing != value)
    {
        m_paragraphSpacing = value;
        m_dirty = true;
    }
}

void RawText::maxWidth(float value)
{
    if (m_maxWidth != value)
    {
        m_maxWidth = value;
        m_dirty = true;
    }
}

void RawText::maxHeight(float value)
{
    if (m_maxHeight != value)
    {
        m_maxHeight = value;
        m_dirty = true;
    }
}

void RawText::update()
{
    for (RenderStyle& style : m_styles)
    {
        style.path->rewind();
        style.isEmpty = true;
    }
    m_renderStyles.clear();
    if (m_styled.empty())
    {
        return;
    }
    auto runs = m_styled.runs();
    m_shape = runs[0].font->shapeText(m_styled.unichars(), runs);
    m_lines =
        Text::BreakLines(m_shape, m_sizing == TextSizing::autoWidth ? -1.0f : m_maxWidth, m_align);

    m_orderedLines.clear();
    m_ellipsisRun = {};

    // build render styles.
    if (m_shape.empty())
    {
        m_bounds = AABB(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    // Build up ordered runs as we go.
    int paragraphIndex = 0;
    float y = 0.0f;
    float minY = 0.0f;
    float measuredWidth = 0.0f;
    if (m_origin == TextOrigin::baseline && !m_lines.empty() && !m_lines[0].empty())
    {
        y -= m_lines[0][0].baseline;
        minY = y;
    }

    int ellipsisLine = -1;
    bool isEllipsisLineLast = false;
    // Find the line to put the ellipsis on (line before the one that
    // overflows).
    bool wantEllipsis = m_overflow == TextOverflow::ellipsis && m_sizing == TextSizing::fixed;

    int lastLineIndex = -1;
    for (const SimpleArray<GlyphLine>& paragraphLines : m_lines)
    {
        const Paragraph& paragraph = m_shape[paragraphIndex++];
        for (const GlyphLine& line : paragraphLines)
        {
            const GlyphRun& endRun = paragraph.runs[line.endRunIndex];
            const GlyphRun& startRun = paragraph.runs[line.startRunIndex];
            float width = endRun.xpos[line.endGlyphIndex] - startRun.xpos[line.startGlyphIndex] -
                          endRun.letterSpacing;
            if (width > measuredWidth)
            {
                measuredWidth = width;
            }
            lastLineIndex++;
            if (wantEllipsis && y + line.bottom <= m_maxHeight)
            {
                ellipsisLine++;
            }
        }

        if (!paragraphLines.empty())
        {
            y += paragraphLines.back().bottom;
        }
        y += m_paragraphSpacing;
    }
    if (wantEllipsis && ellipsisLine == -1)
    {
        // Nothing fits, just show the first line and ellipse it.
        ellipsisLine = 0;
    }
    isEllipsisLineLast = lastLineIndex == ellipsisLine;

    int lineIndex = 0;
    paragraphIndex = 0;
    switch (m_sizing)
    {
        case TextSizing::autoWidth:
            m_bounds = AABB(0.0f, minY, measuredWidth, std::max(minY, y - m_paragraphSpacing));
            break;
        case TextSizing::autoHeight:
            m_bounds = AABB(0.0f, minY, m_maxWidth, std::max(minY, y - m_paragraphSpacing));
            break;
        case TextSizing::fixed:
            m_bounds = AABB(0.0f, minY, m_maxWidth, minY + m_maxHeight);
            break;
    }

    // Build the clip path if we want it.
    if (m_overflow == TextOverflow::clipped)
    {
        if (m_clipRenderPath == nullptr)
        {
            m_clipRenderPath = m_factory->makeEmptyRenderPath();
        }
        else
        {
            m_clipRenderPath->rewind();
        }

        m_clipRenderPath->addRect(m_bounds.minX,
                                  m_bounds.minY,
                                  m_bounds.width(),
                                  m_bounds.height());
    }
    else
    {
        m_clipRenderPath = nullptr;
    }

    y = 0;
    if (m_origin == TextOrigin::baseline && !m_lines.empty() && !m_lines[0].empty())
    {
        y -= m_lines[0][0].baseline;
    }
    paragraphIndex = 0;

    for (const SimpleArray<GlyphLine>& paragraphLines : m_lines)
    {
        const Paragraph& paragraph = m_shape[paragraphIndex++];
        for (const GlyphLine& line : paragraphLines)
        {
            switch (m_overflow)
            {
                case TextOverflow::hidden:
                    if (m_sizing == TextSizing::fixed && y + line.bottom > m_maxHeight)
                    {
                        return;
                    }
                    break;
                case TextOverflow::clipped:
                    if (m_sizing == TextSizing::fixed && y + line.top > m_maxHeight)
                    {
                        return;
                    }
                    break;
                default:
                    break;
            }

            if (lineIndex >= m_orderedLines.size())
            {
                // We need to still compute this line's ordered runs.
                m_orderedLines.emplace_back(OrderedLine(paragraph,
                                                        line,
                                                        m_maxWidth,
                                                        ellipsisLine == lineIndex,
                                                        isEllipsisLineLast,
                                                        &m_ellipsisRun));
            }

            const OrderedLine& orderedLine = m_orderedLines[lineIndex];
            float x = line.startX;
            float renderY = y + line.baseline;
            for (auto glyphItr : orderedLine)
            {
                const GlyphRun* run = std::get<0>(glyphItr);
                size_t glyphIndex = std::get<1>(glyphItr);

                const Font* font = run->font.get();
                const Vec2D& offset = run->offsets[glyphIndex];

                GlyphID glyphId = run->glyphs[glyphIndex];
                float advance = run->advances[glyphIndex];

                RawPath path = font->getPath(glyphId);

                path.transformInPlace(
                    Mat2D(run->size, 0.0f, 0.0f, run->size, x + offset.x, renderY + offset.y));

                x += advance;

                assert(run->styleId < m_styles.size());
                RenderStyle* style = &m_styles[run->styleId];
                assert(style != nullptr);
                path.addTo(style->path.get());

                if (style->isEmpty)
                {
                    // This was the first path added to the style, so let's mark
                    // it in our draw list.
                    style->isEmpty = false;

                    m_renderStyles.push_back(style);
                }
            }
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
        y += m_paragraphSpacing;
    }
}

AABB RawText::bounds()
{
    if (m_dirty)
    {
        update();
        m_dirty = false;
    }
    return m_bounds;
}

void RawText::render(Renderer* renderer, rcp<RenderPaint> paint)
{
    if (m_dirty)
    {
        update();
        m_dirty = false;
    }

    if (m_overflow == TextOverflow::clipped && m_clipRenderPath)
    {
        renderer->save();
        renderer->clipPath(m_clipRenderPath.get());
    }
    for (auto style : m_renderStyles)
    {
        renderer->drawPath(style->path.get(), paint ? paint.get() : style->paint.get());
    }
    if (m_overflow == TextOverflow::clipped && m_clipRenderPath)
    {
        renderer->restore();
    }
}
#endif
