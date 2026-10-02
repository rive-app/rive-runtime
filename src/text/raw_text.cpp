#ifdef WITH_RIVE_TEXT
#include "rive/text/raw_text.hpp"
#include "rive/text_engine.hpp"
#include "rive/factory.hpp"
#include "rive/shapes/paint/color.hpp"

using namespace rive;

RawText::RawText(Factory* factory) : m_factory(factory) {}
bool RawText::empty() const { return m_styled.empty(); }

void RawText::append(const std::string& text,
                     rcp<RenderPaint> paint,
                     rcp<Font> font,
                     float size,
                     float lineHeight,
                     float letterSpacing,
                     ColorInt foregroundColor)
{
    int styleIndex = 0;
    for (RenderStyle& style : m_styles)
    {
        if (style.paint == paint && style.foregroundColor == foregroundColor)
        {
            break;
        }
        styleIndex++;
    }
    if (styleIndex == (int)m_styles.size())
    {
        m_styles.emplace_back();
        auto& style = m_styles.back();
        style.paint = paint;
        style.isEmpty = true;
        style.foregroundColor = foregroundColor;
    }
    m_styled.append(font, size, lineHeight, letterSpacing, text, styleIndex);
    markDirty();
}

void RawText::clear()
{
    m_styled.clear();
    m_styles.clear();
    markDirty();
}

void RawText::markDirty()
{
    m_dirty = true;
    m_lookupDirty = true;
    m_revision++;
}

TextSizing RawText::sizing() const { return m_sizing; }

TextOverflow RawText::overflow() const { return m_overflow; }

TextAlign RawText::align() const { return m_align; }

RawText::LogicalAlign RawText::logicalAlign() const { return m_logicalAlign; }

TextWrap RawText::wrap() const { return m_wrap; }

TextWordBreak RawText::wordBreak() const { return m_wordBreak; }

TextOrigin RawText::origin() const { return m_origin; }

int RawText::directionFlag() const { return m_directionFlag; }

float RawText::maxWidth() const { return m_maxWidth; }

float RawText::maxHeight() const { return m_maxHeight; }

float RawText::paragraphSpacing() const { return m_paragraphSpacing; }

void RawText::sizing(TextSizing value)
{
    if (m_sizing != value)
    {
        m_sizing = value;
        markDirty();
    }
}

void RawText::overflow(TextOverflow value)
{
    if (m_overflow != value)
    {
        m_overflow = value;
        markDirty();
    }
}

void RawText::align(TextAlign value)
{
    if (m_align != value)
    {
        m_align = value;
        markDirty();
    }
}

void RawText::logicalAlign(LogicalAlign value)
{
    if (m_logicalAlign != value)
    {
        m_logicalAlign = value;
        markDirty();
    }
}

void RawText::wrap(TextWrap value)
{
    if (m_wrap != value)
    {
        m_wrap = value;
        markDirty();
    }
}

void RawText::wordBreak(TextWordBreak value)
{
    if (m_wordBreak != value)
    {
        m_wordBreak = value;
        markDirty();
    }
}

void RawText::origin(TextOrigin value)
{
    if (m_origin != value)
    {
        m_origin = value;
        markDirty();
    }
}

void RawText::directionFlag(int value)
{
    if (m_directionFlag != value)
    {
        m_directionFlag = value;
        markDirty();
    }
}

int RawText::alignIndex() const
{
    return m_logicalAlign == LogicalAlign::none ? (int)m_align
                                                : 2 + (int)m_logicalAlign;
}

void RawText::alignIndex(int value)
{
    if (value >= 0 && value <= 2)
    {
        align((TextAlign)value);
        logicalAlign(LogicalAlign::none);
    }
    else if (value == 3 || value == 4)
    {
        logicalAlign((LogicalAlign)(value - 2));
    }
}

void RawText::directionIndex(int value)
{
    if (value >= 0 && value <= 2)
    {
        directionFlag(value - 1);
    }
}

void RawText::sizingIndex(int value)
{
    if (value >= 0 && value <= (int)TextSizing::fixed)
    {
        sizing((TextSizing)value);
    }
}

void RawText::overflowIndex(int value)
{
    // fit and fitFontSize live in the Text component, not here.
    if (value >= 0 && value <= (int)TextOverflow::ellipsis)
    {
        overflow((TextOverflow)value);
    }
}

void RawText::wrapIndex(int value)
{
    if (value >= 0 && value <= (int)TextWrap::noWrap)
    {
        wrap((TextWrap)value);
    }
}

void RawText::wordBreakIndex(int value)
{
    if (value >= 0 && value <= (int)TextWordBreak::breakAll)
    {
        wordBreak((TextWordBreak)value);
    }
}

void RawText::originIndex(int value)
{
    if (value >= 0 && value <= (int)TextOrigin::baseline)
    {
        origin((TextOrigin)value);
    }
}

void RawText::paragraphSpacing(float value)
{
    if (m_paragraphSpacing != value)
    {
        m_paragraphSpacing = value;
        markDirty();
    }
}

void RawText::maxWidth(float value)
{
    if (m_maxWidth != value)
    {
        m_maxWidth = value;
        markDirty();
    }
}

void RawText::maxHeight(float value)
{
    if (m_maxHeight != value)
    {
        m_maxHeight = value;
        markDirty();
    }
}

void RawText::update()
{
    for (RenderStyle& style : m_styles)
    {
        style.path.rewind();
        style.isEmpty = true;
    }
    m_renderStyles.clear();
    m_drawCommands.clear();
    m_orderedLines.clear();
    m_lineDirections.clear();
    m_lineGlyphCounts.clear();
    if (m_styled.empty())
    {
        // Readers of the layout must not see the previous text's lines.
        m_shape = SimpleArray<Paragraph>();
        m_lines = SimpleArray<SimpleArray<GlyphLine>>();
        m_bounds = AABB(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }
    auto runs = m_styled.runs();

    m_shape =
        runs[0].font->shapeText(m_styled.unichars(), runs, m_directionFlag);
    TextAlign align = m_align;
    if (m_logicalAlign != LogicalAlign::none && !m_shape.empty())
    {
        bool rtl = m_shape[0].baseDirection() == TextDirection::rtl;
        align = (m_logicalAlign == LogicalAlign::start) != rtl
                    ? TextAlign::left
                    : TextAlign::right;
    }
    m_lines =
        Text::BreakLines(m_shape,
                         m_sizing == TextSizing::autoWidth ? -1.0f : m_maxWidth,
                         align,
                         m_wrap,
                         m_wordBreak);

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
    if (m_origin == TextOrigin::baseline && !m_lines.empty() &&
        !m_lines[0].empty())
    {
        y -= m_lines[0][0].baseline;
        minY = y;
    }

    int ellipsisLine = -1;
    bool isEllipsisLineLast = false;
    // Find the line to put the ellipsis on (line before the one that
    // overflows).
    bool wantEllipsis =
        m_overflow == TextOverflow::ellipsis && m_sizing == TextSizing::fixed;

    int lastLineIndex = -1;
    for (const SimpleArray<GlyphLine>& paragraphLines : m_lines)
    {
        const Paragraph& paragraph = m_shape[paragraphIndex++];
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
    switch (m_sizing)
    {
        case TextSizing::autoWidth:
            m_bounds = AABB(0.0f,
                            minY,
                            measuredWidth,
                            std::max(minY, y - m_paragraphSpacing));
            break;
        case TextSizing::autoHeight:
            m_bounds = AABB(0.0f,
                            minY,
                            m_maxWidth,
                            std::max(minY, y - m_paragraphSpacing));
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
    if (m_origin == TextOrigin::baseline && !m_lines.empty() &&
        !m_lines[0].empty())
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
                    if (m_sizing == TextSizing::fixed &&
                        y + line.bottom > m_maxHeight)
                    {
                        return;
                    }
                    break;
                case TextOverflow::clipped:
                    if (m_sizing == TextSizing::fixed &&
                        y + line.top > m_maxHeight)
                    {
                        return;
                    }
                    break;
                default:
                    break;
            }

            float renderY = y + line.baseline;
            if (lineIndex >= m_orderedLines.size())
            {
                // We need to still compute this line's ordered runs.
                m_orderedLines.emplace_back(
                    OrderedLine(paragraph,
                                line,
                                m_maxWidth,
                                ellipsisLine == lineIndex,
                                isEllipsisLineLast,
                                &m_ellipsisRun,
                                renderY));
            }

            const OrderedLine& orderedLine = m_orderedLines[lineIndex];
            m_lineDirections.push_back((uint8_t)paragraph.baseDirection());
            uint32_t lineGlyphs = 0;
            float x = line.startX;

            for (auto glyphItr : orderedLine)
            {
                lineGlyphs++;
                const GlyphRun* run = std::get<0>(glyphItr);
                size_t glyphIndex = std::get<1>(glyphItr);

                const Font* font = run->font.get();
                const Vec2D& offset = run->offsets[glyphIndex];

                GlyphID glyphId = run->glyphs[glyphIndex];
                float advance = run->advances[glyphIndex];

                assert(run->styleId < m_styles.size());
                RenderStyle* style = &m_styles[run->styleId];
                assert(style != nullptr);

                Mat2D transform(run->size,
                                0.0f,
                                0.0f,
                                run->size,
                                x + offset.x,
                                renderY + offset.y);

                x += advance;

                if (font->isColorGlyph(glyphId))
                {
                    RawTextDrawCommand cmd;
                    cmd.type = RawTextDrawCommand::kColorGlyph;
                    cmd.colorGlyph = {run->font,
                                      glyphId,
                                      transform,
                                      style->foregroundColor};
                    m_drawCommands.push_back(std::move(cmd));
                }
                else
                {
                    RawPath path = font->getPath(glyphId);
                    style->path.addPathClockwise(path, &transform);

                    if (style->isEmpty)
                    {
                        style->isEmpty = false;
                        m_renderStyles.push_back(style);

                        RawTextDrawCommand cmd;
                        cmd.type = RawTextDrawCommand::kStylePath;
                        cmd.style = style;
                        m_drawCommands.push_back(std::move(cmd));
                    }
                }
            }
            m_lineGlyphCounts.push_back(lineGlyphs);
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
    for (auto& cmd : m_drawCommands)
    {
        if (cmd.type == RawTextDrawCommand::kStylePath)
        {
            auto renderPaint = paint ? paint.get() : cmd.style->paint.get();
            if (renderPaint != nullptr)
            {
                renderer->drawPath(cmd.style->path.renderPath(m_factory),
                                   renderPaint);
            }
        }
        else
        {
            auto& info = cmd.colorGlyph;
            drawColorGlyph(renderer,
                           *info.font,
                           info.glyphId,
                           info.transform,
                           info.foregroundColor);
        }
    }
    if (m_overflow == TextOverflow::clipped && m_clipRenderPath)
    {
        renderer->restore();
    }
}

void RawText::drawColorGlyph(Renderer* renderer,
                             const Font& font,
                             GlyphID glyphId,
                             const Mat2D& transform,
                             ColorInt foregroundColor)
{
    std::vector<Font::ColorGlyphLayer> layers;
    if (font.getColorLayers(glyphId, layers, foregroundColor) == 0)
    {
        return;
    }
    renderer->save();
    renderer->transform(transform);
    for (auto& layer : layers)
    {
        drawColorGlyphLayer(renderer, m_factory, layer, 1.0f);
    }
    renderer->restore();
}

RawPath RawText::glyphPath(const Font& font, GlyphID glyphId)
{
    ShapePaintPath path;
    path.addPathClockwise(font.getPath(glyphId), nullptr);
    return *path.rawPath();
}

const std::vector<OrderedLine>& RawText::orderedLines()
{
    if (m_dirty)
    {
        update();
        m_dirty = false;
    }
    return m_orderedLines;
}

float RawText::lineTop(uint32_t line)
{
    const OrderedLine& orderedLine = orderedLines()[line];
    const GlyphLine& glyphLine = orderedLine.glyphLine();
    return orderedLine.y() - glyphLine.baseline + glyphLine.top;
}

TextDirection RawText::lineDirection(uint32_t line)
{
    orderedLines();
    return (TextDirection)m_lineDirections[line];
}

uint32_t RawText::lineGlyphCount(uint32_t line)
{
    orderedLines();
    return m_lineGlyphCounts[line];
}

TextLayoutView RawText::layoutView()
{
    orderedLines();
    if (m_lookupDirty)
    {
        m_glyphLookup.compute(m_styled.unichars(), m_shape);
        m_lookupDirty = false;
    }
    return TextLayoutView(m_shape,
                          m_lines,
                          m_orderedLines,
                          m_glyphLookup,
                          (uint32_t)m_styled.unichars().size());
}

RenderPaint* RawText::stylePaint(uint16_t styleId) const
{
    return styleId < m_styles.size() ? m_styles[styleId].paint.get() : nullptr;
}

ColorInt RawText::styleForegroundColor(uint16_t styleId) const
{
    return styleId < m_styles.size() ? m_styles[styleId].foregroundColor
                                     : 0xFF000000;
}
#endif
