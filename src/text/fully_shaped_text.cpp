#ifdef WITH_RIVE_TEXT
#include "rive/text/fully_shaped_text.hpp"
#include "rive/text/text.hpp"
#include <algorithm>
#include <limits>

using namespace rive;

namespace
{
// Unicode bidi class B: where the shaper starts a new paragraph.
bool isParagraphSeparator(Unichar c)
{
  return c == 0x000A || c == 0x000D || (c >= 0x001C && c <= 0x001E) ||
         c == 0x0085 || c == 0x2029;
}

// File, group and record separators.
bool isInformationSeparator(Unichar c) { return c >= 0x001C && c <= 0x001E; }

// CR LF ends a single paragraph.
void appendParagraphStarts(
    const Unichar* text,
    size_t size,
    uint32_t base,
    std::vector<uint32_t>& starts)
{
  for (size_t i = 0; i < size; i++)
  {
    Unichar c = text[i];
    if (!isParagraphSeparator(c))
    {
      continue;
    }
    if (c == 0x000D && i + 1 < size && text[i + 1] == 0x000A)
    {
      i++;
    }
    if (i + 1 < size)
    {
      starts.push_back(base + (uint32_t)(i + 1));
    }
  }
}

// Puts added in place of array's [first, first + removed), or as many empty
// elements when there are none to move in.
template <typename T>
void replaceRange(
    SimpleArray<T>& array,
    size_t first,
    size_t removed,
    T* added,
    size_t addedCount)
{
  if (removed != addedCount)
  {
    SimpleArray<T> replaced(array.size() - removed + addedCount);
    for (size_t i = 0; i < first; i++)
    {
      replaced[i] = std::move(array[i]);
    }
    for (size_t i = first + removed; i < array.size(); i++)
    {
      replaced[i - removed + addedCount] = std::move(array[i]);
    }
    array = std::move(replaced);
  }
  for (size_t i = 0; i < addedCount; i++)
  {
    array[first + i] = added == nullptr ? T() : std::move(added[i]);
  }
}

void offsetTextIndices(Paragraph& paragraph, int64_t offset)
{
  for (GlyphRun& run : paragraph.runs)
  {
    for (uint32_t& index : run.textIndices)
    {
      index = (uint32_t)(index + offset);
    }
    for (uint32_t& index : run.joiners)
    {
      index = (uint32_t)(index + offset);
    }
  }
}

} // namespace

bool FullyShapedText::Layout::breaksLike(const Layout& other) const
{
  return breakWidth() == other.breakWidth() && alignment == other.alignment &&
         wrap == other.wrap && alignWidth == other.alignWidth;
}

bool FullyShapedText::Layout::operator==(const Layout& other) const
{
  return breaksLike(other) && sizing == other.sizing &&
         maxWidth == other.maxWidth && maxHeight == other.maxHeight &&
         origin == other.origin && overflow == other.overflow &&
         paragraphSpacing == other.paragraphSpacing &&
         verticalAlignment == other.verticalAlignment &&
         alignHeight == other.alignHeight;
}

void FullyShapedText::ParagraphExtent::moveDown(
    float& y,
    float paragraphSpacing) const
{
  if (lineCount != 0)
  {
    y += bottom;
  }
  y += paragraphSpacing;
}

namespace
{
// Whether OrderedLine::justify widens a line of the given width to fill
// width: it needs a whitespace glyph and room to grow.
bool stretches(
    const Paragraph& paragraph,
    const GlyphLine& line,
    Span<const Unichar> text,
    float lineWidth,
    float width)
{
  if (width <= lineWidth)
  {
    return false;
  }
  for (uint32_t r = line.startRunIndex; r <= line.endRunIndex; r++)
  {
    const GlyphRun& run = paragraph.runs[r];
    uint32_t begin = r == line.startRunIndex ? line.startGlyphIndex : 0;
    uint32_t end = r == line.endRunIndex ? line.endGlyphIndex
                                         : (uint32_t)run.glyphs.size();
    for (uint32_t g = begin; g < end; g++)
    {
      uint32_t textIndex = run.textIndices[g];
      if (textIndex < text.size() && isWhiteSpace(text[textIndex]))
      {
        return true;
      }
    }
  }
  return false;
}
} // namespace

void FullyShapedText::measureParagraphs(
    const SimpleArray<Paragraph>& paragraphs,
    const SimpleArray<SimpleArray<GlyphLine>>& paragraphLines,
    size_t first,
    size_t end,
    const Layout& layout,
    Span<const Unichar> text,
    std::vector<ParagraphExtent>& extents)
{
  bool justifies = layout.justifies();
  float justifyWidth = layout.justifyWidth();
  for (size_t i = first; i < end; i++)
  {
    const Paragraph& paragraph = paragraphs[i];
    const SimpleArray<GlyphLine>& lines = paragraphLines[i];
    // Lines don't start at 0 when they're aligned center/right, so track the
    // real horizontal extents rather than assuming [0, widest line].
    ParagraphExtent extent = {
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        lines.empty() ? 0.0f : lines.back().bottom,
        (uint32_t)lines.size(),
    };
    for (const GlyphLine& line : lines)
    {
      const GlyphRun& endRun = paragraph.runs[line.endRunIndex];
      const GlyphRun& startRun = paragraph.runs[line.startRunIndex];
      float width = endRun.xpos[line.endGlyphIndex] -
                    startRun.xpos[line.startGlyphIndex];
      float startX = line.startX;
      // A justified line spans the width from 0 (see orderLines). The
      // ellipsis line, which isn't justified, is only known once the lines
      // are measured; a field cut short by one is as wide as this anyway.
      if (justifies && &line != &lines.back() &&
          stretches(paragraph, line, text, width, justifyWidth))
      {
        startX = 0.0f;
        width = justifyWidth;
      }
      extent.minX = std::min(extent.minX, startX);
      extent.maxX = std::max(extent.maxX, startX + width);
    }
    extents[i] = extent;
  }
}

FullyShapedText::LineMetrics FullyShapedText::measureLines(
    const std::vector<ParagraphExtent>& extents,
    const SimpleArray<SimpleArray<GlyphLine>>& paragraphLines,
    const Layout& layout)
{
  LineMetrics metrics;
  float y = 0.0f;
  float minY = 0.0f;
  float minX = std::numeric_limits<float>::max();
  float maxX = -std::numeric_limits<float>::max();
  if (layout.origin == TextOrigin::baseline && !paragraphLines.empty() &&
      !paragraphLines[0].empty())
  {
    y -= paragraphLines[0][0].baseline;
    minY = y;
  }

  int ellipsisLine = -1;
  // Find the line to put the ellipsis on (line before the one that
  // overflows).
  bool wantEllipsis = layout.overflow == TextOverflow::ellipsis &&
                      layout.sizing == TextSizing::fixed;

  int lastLineIndex = -1;
  for (size_t i = 0; i < extents.size(); i++)
  {
    const ParagraphExtent& extent = extents[i];
    minX = std::min(minX, extent.minX);
    maxX = std::max(maxX, extent.maxX);
    lastLineIndex += extent.lineCount;
    if (wantEllipsis)
    {
      for (const GlyphLine& line : paragraphLines[i])
      {
        if (y + line.bottom <= layout.maxHeight)
        {
          ellipsisLine++;
        }
      }
    }
    extent.moveDown(y, layout.paragraphSpacing);
  }
  if (wantEllipsis && ellipsisLine == -1)
  {
    // Nothing fits, just show the first line and ellipse it.
    ellipsisLine = 0;
  }
  metrics.ellipsisLine = ellipsisLine;
  metrics.isEllipsisLineLast = lastLineIndex == ellipsisLine;

  if (minX > maxX)
  {
    // Paragraphs but no lines.
    minX = maxX = 0.0f;
  }
  float maxY = std::max(minY, y - layout.paragraphSpacing);

  // Align the block within the field the same way lines align within it
  // horizontally: only while it fits, so text taller than the field stays
  // at the top and leaves vertical scrolling alone.
  float slack = layout.alignHeight - (maxY - minY);
  if (slack > 0.0f)
  {
    switch (layout.verticalAlignment)
    {
      case VerticalTextAlign::top:
        break;
      case VerticalTextAlign::middle:
        metrics.verticalOffset = slack / 2.0f;
        break;
      case VerticalTextAlign::bottom:
        metrics.verticalOffset = slack;
        break;
    }
  }

  metrics.bounds = AABB(
      minX,
      minY + metrics.verticalOffset,
      maxX,
      maxY + metrics.verticalOffset);
  return metrics;
}

void FullyShapedText::shape(
    Span<Unichar> text,
    Span<TextRun> runs,
    TextSizing sizing,
    float maxWidth,
    float maxHeight,
    TextAlign alignment,
    TextWrap wrap,
    TextOrigin origin,
    TextOverflow overflow,
    float paragraphSpacing,
    float alignWidth,
    VerticalTextAlign verticalAlignment,
    float alignHeight)
{
  shapeParagraphs(text, runs);
  layout(
      sizing,
      maxWidth,
      maxHeight,
      alignment,
      wrap,
      origin,
      overflow,
      paragraphSpacing,
      alignWidth,
      verticalAlignment,
      alignHeight);
}

void FullyShapedText::dropLines()
{
  m_paragraphLines = SimpleArray<SimpleArray<GlyphLine>>();
  m_linesBroken = false;
  m_staleFirst = m_staleEnd = 0;
  m_orderedLines.clear();
  m_ellipsisRun = {};
}

void FullyShapedText::shapeParagraphs(Span<Unichar> text, Span<TextRun> runs)
{
  m_text = text;
  m_paragraphs = runs[0].font->shapeText(text, runs);
  m_glyphLookup.compute(text, m_paragraphs);
  dropLines();

  m_paragraphStarts.clear();
  if (!text.empty())
  {
    m_paragraphStarts.push_back(0);
    appendParagraphStarts(text.data(), text.size(), 0, m_paragraphStarts);
  }
  m_paragraphStarts.push_back((uint32_t)text.size());
  if (m_paragraphStarts.size() != m_paragraphs.size() + 1)
  {
    m_paragraphStarts.clear();
  }
}

bool FullyShapedText::reshapeParagraphs(
    Span<Unichar> text,
    Span<TextRun> runs,
    uint32_t editStart,
    uint32_t oldEnd,
    uint32_t newEnd)
{
  m_text = text;
  size_t count = m_paragraphs.size();
  if (runs.size() != 1 || count == 0 || m_paragraphStarts.size() != count + 1 ||
      editStart > oldEnd || editStart > newEnd)
  {
    return false;
  }
  uint32_t oldLength = m_paragraphStarts.back();
  int64_t delta = (int64_t)newEnd - (int64_t)oldEnd;
  if (oldEnd > oldLength || (int64_t)oldLength + delta != (int64_t)text.size())
  {
    return false;
  }

  // Includes the paragraph before an edit starting at a boundary, which the
  // edit can join (an LF after a CR).
  auto first = m_paragraphStarts.begin();
  auto last = m_paragraphStarts.end() - 1;
  size_t p0 = std::upper_bound(first, last, editStart) - first - 1;
  if (p0 > 0 && m_paragraphStarts[p0] == editStart)
  {
    p0--;
  }
  size_t p1 = std::upper_bound(first, last, oldEnd) - first - 1;
  uint32_t spanStart = m_paragraphStarts[p0];
  uint32_t newSpanEnd = (uint32_t)(m_paragraphStarts[p1 + 1] + delta);
  uint32_t spanLength = newSpanEnd - spanStart;
  bool spanAtEnd = p1 + 1 == count;
  if (spanLength > text.size() / 2)
  {
    return false;
  }
  // These end a paragraph without ending the line break context around
  // them, so the paragraphs on either side don't break on their own.
  if ((spanStart != 0 && isInformationSeparator(text[spanStart - 1])) ||
      (newSpanEnd != 0 && isInformationSeparator(text[newSpanEnd - 1])))
  {
    return false;
  }

  SimpleArray<Paragraph> shaped;
  std::vector<uint32_t> spanStarts;
  if (spanLength != 0)
  {
    TextRun run = runs[0];
    run.unicharCount = spanLength;
    shaped = run.font->shapeText(
        Span<const Unichar>(text.data() + spanStart, spanLength),
        Span<const TextRun>(&run, 1));
    spanStarts.push_back(spanStart);
    appendParagraphStarts(
        text.data() + spanStart,
        spanLength,
        spanStart,
        spanStarts);
    if (spanStarts.size() != shaped.size())
    {
      return false;
    }
    if (!spanAtEnd)
    {
      // The shaper gives the text's last run two closing breaks, which a
      // run mid text doesn't have.
      Paragraph& lastParagraph = shaped[shaped.size() - 1];
      if (lastParagraph.runs.empty())
      {
        return false;
      }
      GlyphRun& tail = lastParagraph.runs[lastParagraph.runs.size() - 1];
      if (tail.breaks.size() < 2)
      {
        return false;
      }
      SimpleArrayBuilder<uint32_t> breaks(tail.breaks.size() - 2);
      for (size_t i = 0; i + 2 < tail.breaks.size(); i++)
      {
        breaks.add(tail.breaks[i]);
      }
      tail.breaks = std::move(breaks);
    }
  }

  size_t removed = p1 - p0 + 1;
  size_t added = shaped.size();
  uint32_t oldSpanLength = m_paragraphStarts[p1 + 1] - spanStart;
  // The paragraphs kept keep their lines, for layout() to lay out the rest.
  if (m_linesBroken && m_staleFirst == m_staleEnd)
  {
    replaceRange<SimpleArray<GlyphLine>>(
        m_paragraphLines,
        p0,
        removed,
        nullptr,
        added);
    m_staleFirst = p0;
    m_staleEnd = p0 + added;

    auto replaced = m_extents.begin() + p0;
    m_staleExtents.assign(replaced, replaced + removed);
    m_extents.insert(
        m_extents.erase(replaced, replaced + removed),
        added,
        ParagraphExtent());
  }
  else
  {
    dropLines();
  }

  for (Paragraph& paragraph : shaped)
  {
    offsetTextIndices(paragraph, spanStart);
  }
  if (delta != 0)
  {
    for (size_t i = p1 + 1; i < count; i++)
    {
      offsetTextIndices(m_paragraphs[i], delta);
    }
  }
  replaceRange(m_paragraphs, p0, removed, shaped.data(), added);

  auto start = m_paragraphStarts.begin() + p0;
  start = m_paragraphStarts.insert(
      m_paragraphStarts.erase(start, start + removed),
      spanStarts.begin(),
      spanStarts.end());
  for (start += added; start != m_paragraphStarts.end(); ++start)
  {
    *start = (uint32_t)(*start + delta);
  }

  m_glyphLookup.replace(
      spanStart,
      oldSpanLength,
      spanLength,
      Span<const Paragraph>(m_paragraphs.data() + p0, added));
  return true;
}

AABB FullyShapedText::measure(
    TextSizing sizing,
    float maxWidth,
    float maxHeight,
    TextAlign alignment,
    TextWrap wrap,
    TextOrigin origin,
    TextOverflow overflow,
    float paragraphSpacing,
    float alignWidth,
    VerticalTextAlign verticalAlignment,
    float alignHeight) const
{
  if (m_paragraphs.empty())
  {
    return AABB(0.0f, 0.0f, 0.0f, 0.0f);
  }
  Layout layout = {
      sizing,
      maxWidth,
      maxHeight,
      alignment,
      wrap,
      origin,
      overflow,
      paragraphSpacing,
      alignWidth,
      verticalAlignment,
      alignHeight,
  };
  if (m_linesBroken && m_staleFirst == m_staleEnd &&
      layout.breaksLike(m_layout))
  {
    return measureLines(m_extents, m_paragraphLines, layout).bounds;
  }
  auto paragraphLines = Text::BreakLines(
      m_paragraphs,
      layout.breakWidth(),
      alignment,
      wrap,
      TextWordBreak::breakWord,
      alignWidth);
  std::vector<ParagraphExtent> extents(m_paragraphs.size());
  measureParagraphs(
      m_paragraphs,
      paragraphLines,
      0,
      m_paragraphs.size(),
      layout,
      m_text,
      extents);
  return measureLines(extents, paragraphLines, layout).bounds;
}

void FullyShapedText::layout(
    TextSizing sizing,
    float maxWidth,
    float maxHeight,
    TextAlign alignment,
    TextWrap wrap,
    TextOrigin origin,
    TextOverflow overflow,
    float paragraphSpacing,
    float alignWidth,
    VerticalTextAlign verticalAlignment,
    float alignHeight)
{
  Layout layout = {
      sizing,
      maxWidth,
      maxHeight,
      alignment,
      wrap,
      origin,
      overflow,
      paragraphSpacing,
      alignWidth,
      verticalAlignment,
      alignHeight,
  };
  size_t count = m_paragraphs.size();
  size_t first = m_staleFirst;
  size_t end = m_staleEnd;
  std::vector<ParagraphExtent> replaced;
  replaced.swap(m_staleExtents);
  float width = layout.breakWidth();
  // Without a width every paragraph aligns to the widest line of them all.
  bool sameBreaks = m_linesBroken && width != -1.0f &&
                    layout.breaksLike(m_layout);
  if (sameBreaks)
  {
    Text::BreakLines(
        m_paragraphs,
        m_paragraphLines,
        first,
        end,
        width,
        alignment,
        wrap,
        TextWordBreak::breakWord,
        alignWidth);
  }
  else
  {
    m_paragraphLines = Text::BreakLines(
        m_paragraphs,
        width,
        alignment,
        wrap,
        TextWordBreak::breakWord,
        alignWidth);
    first = 0;
    end = count;
    m_extents.resize(count);
  }
  measureParagraphs(
      m_paragraphs,
      m_paragraphLines,
      first,
      end,
      layout,
      m_text,
      m_extents);
  // Ordered lines patch only when all of them show, which fixed sizing can
  // prevent, and no new first line moved the baseline they hang from.
  bool patch = sameBreaks && layout == m_layout &&
               sizing != TextSizing::fixed &&
               (origin != TextOrigin::baseline || first != 0);
  m_layout = layout;
  m_linesBroken = true;
  m_staleFirst = m_staleEnd = 0;
  m_ellipsisRun = {};
  m_lineChange = LineChange();

  // build render styles.
  if (count == 0)
  {
    m_orderedLines.clear();
    m_verticalOffset = 0.0f;
    m_bounds = AABB(0.0f, 0.0f, 0.0f, 0.0f);
    return;
  }

  LineMetrics metrics = measureLines(m_extents, m_paragraphLines, layout);
  patch = patch && metrics.verticalOffset == m_verticalOffset;
  m_verticalOffset = metrics.verticalOffset;
  m_bounds = metrics.bounds;

  // Left unshifted so the overflow checks below still measure the text
  // against the box; only the line positions carry the alignment offset.
  float y = 0.0f;
  if (origin == TextOrigin::baseline && !m_paragraphLines[0].empty())
  {
    y -= m_paragraphLines[0][0].baseline;
  }
  if (patch)
  {
    patchOrderedLines(first, end, replaced, y);
    return;
  }
  m_orderedLines.clear();
  orderLines(
      0,
      count,
      metrics.ellipsisLine,
      metrics.isEllipsisLineLast,
      y,
      m_orderedLines);
}

void FullyShapedText::orderLines(
    size_t first,
    size_t end,
    int ellipsisLine,
    bool isEllipsisLineLast,
    float& y,
    std::vector<OrderedLine>& lines)
{
  for (size_t i = first; i < end; i++)
  {
    const Paragraph& paragraph = m_paragraphs[i];
    for (const GlyphLine& line : m_paragraphLines[i])
    {
      switch (m_layout.overflow)
      {
        case TextOverflow::hidden:
          if (m_layout.sizing == TextSizing::fixed &&
              y + line.bottom > m_layout.maxHeight)
          {
            return;
          }
          break;
        case TextOverflow::clipped:
          if (m_layout.sizing == TextSizing::fixed &&
              y + line.top > m_layout.maxHeight)
          {
            return;
          }
          break;
        default:
          break;
      }

      int lineIndex = (int)lines.size();
      lines.emplace_back(OrderedLine(
          paragraph,
          line,
          m_layout.maxWidth,
          m_layout.alignment,
          ellipsisLine == lineIndex,
          isEllipsisLineLast,
          &m_ellipsisRun,
          y + line.baseline + m_verticalOffset));
      // A justified paragraph's lines fill the width, all but its last.
      if (m_layout.justifies() && &line != &m_paragraphLines[i].back() &&
          ellipsisLine != lineIndex)
      {
        lines.back().justify(m_text, m_layout.justifyWidth());
      }

      if (lineIndex == ellipsisLine)
      {
        return;
      }
    }
    m_extents[i].moveDown(y, m_layout.paragraphSpacing);
  }
}

void FullyShapedText::patchOrderedLines(
    size_t first,
    size_t end,
    const std::vector<ParagraphExtent>& replaced,
    float y)
{
  // Down to the first stale paragraph the lines are as they were.
  size_t firstLine = 0;
  for (size_t i = 0; i < first; i++)
  {
    firstLine += m_extents[i].lineCount;
    m_extents[i].moveDown(y, m_layout.paragraphSpacing);
  }
  size_t removed = 0;
  float oldY = y;
  for (const ParagraphExtent& extent : replaced)
  {
    removed += extent.lineCount;
    extent.moveDown(oldY, m_layout.paragraphSpacing);
  }

  std::vector<OrderedLine> lines;
  orderLines(first, end, -1, false, y, lines);
  size_t added = lines.size();
  size_t common = std::min(removed, added);
  auto at = m_orderedLines.begin() + firstLine;
  std::move(lines.begin(), lines.begin() + common, at);
  if (added > common)
  {
    m_orderedLines.insert(
        at + common,
        std::make_move_iterator(lines.begin() + common),
        std::make_move_iterator(lines.end()));
  }
  else
  {
    m_orderedLines.erase(at + common, at + removed);
  }
  m_lineChange =
      {(uint32_t)firstLine, (uint32_t)removed, (uint32_t)added, false};
  // The lines kept read whitespace from the text they were justified in,
  // which the edit may have moved.
  for (OrderedLine& line : m_orderedLines)
  {
    line.justifiedText(m_text);
  }

  // The lines after only move when the stale ones changed height.
  if (y == oldY)
  {
    return;
  }
  size_t lineIndex = firstLine + added;
  for (size_t i = end; i < m_paragraphs.size(); i++)
  {
    for (const GlyphLine& line : m_paragraphLines[i])
    {
      m_orderedLines[lineIndex++].y(y + line.baseline + m_verticalOffset);
    }
    m_extents[i].moveDown(y, m_layout.paragraphSpacing);
  }
}
#endif