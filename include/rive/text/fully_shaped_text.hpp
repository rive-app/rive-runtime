#ifndef _RIVE_FULLY_SHAPED_TEXT_HPP_
#define _RIVE_FULLY_SHAPED_TEXT_HPP_

#include "rive/span.hpp"
#include "rive/text/utf.hpp"
#include "rive/text_engine.hpp"
#include "rive/text/glyph_lookup.hpp"
#include "rive/text/text_layout_view.hpp"

namespace rive
{
// Structure containing all the necessary data to interact with (draw/edit)
// a block (multiple paragraphs) of text.
class FullyShapedText
{
public:
  // Paragraphs as returned by the shaper.
  const SimpleArray<Paragraph>& paragraphs() const { return m_paragraphs; }

  // Lines as computed by the line breaker.
  const SimpleArray<SimpleArray<GlyphLine>>& paragraphLines() const
  {
    return m_paragraphLines;
  }

  // Lines with glyphs re-ordered into visual (bidi dictated) order.
  const std::vector<OrderedLine>& orderedLines() const
  {
    return m_orderedLines;
  }

  // Lookup table finding glyphs by text index.
  const GlyphLookup& glyphLookup() const { return m_glyphLookup; }

  const AABB& bounds() const { return m_bounds; }

  bool hasValidBounds() const { return !m_bounds.isEmptyOrNaN(); }

  uint32_t lineCount() const { return (uint32_t)m_orderedLines.size(); }

  operator TextLayoutView() const
  {
    auto end = m_glyphLookup.lastCodePointIndex();
    return TextLayoutView(
        m_paragraphs,
        m_paragraphLines,
        m_orderedLines,
        m_glyphLookup,
        end == 0 ? 0 : end - 1);
  }

  /// How far every line was pushed down to align the text within
  /// alignHeight. Already baked into the ordered lines' y and into the
  /// bounds; anything that walks the lines itself has to start here.
  float verticalOffset() const { return m_verticalOffset; }

  void shape(
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
      float alignWidth = 0.0f,
      VerticalTextAlign verticalAlignment = VerticalTextAlign::top,
      float alignHeight = 0.0f);

  /// Lines stay empty until layout() runs.
  void shapeParagraphs(Span<Unichar> text, Span<TextRun> runs);

  /// Shapes only the paragraphs touched by [editStart, oldEnd) becoming
  /// [editStart, newEnd), for layout() to lay out. False and no change if not.
  bool reshapeParagraphs(
      Span<Unichar> text,
      Span<TextRun> runs,
      uint32_t editStart,
      uint32_t oldEnd,
      uint32_t newEnd);

  void layout(
      TextSizing sizing,
      float maxWidth,
      float maxHeight,
      TextAlign alignment,
      TextWrap wrap,
      TextOrigin origin,
      TextOverflow overflow,
      float paragraphSpacing,
      float alignWidth = 0.0f,
      VerticalTextAlign verticalAlignment = VerticalTextAlign::top,
      float alignHeight = 0.0f);

  /// The bounds layout() would give, without changing this shape.
  AABB measure(
      TextSizing sizing,
      float maxWidth,
      float maxHeight,
      TextAlign alignment,
      TextWrap wrap,
      TextOrigin origin,
      TextOverflow overflow,
      float paragraphSpacing,
      float alignWidth = 0.0f,
      VerticalTextAlign verticalAlignment = VerticalTextAlign::top,
      float alignHeight = 0.0f) const;

  /// How the last layout() changed orderedLines(). Lines [first, first +
  /// removed) became added lines, or every line was laid out again.
  struct LineChange
  {
    uint32_t first = 0;
    uint32_t removed = 0;
    uint32_t added = 0;
    bool all = true;
  };
  const LineChange& lineChange() const { return m_lineChange; }

private:
  struct Layout
  {
    TextSizing sizing;
    float maxWidth;
    float maxHeight;
    TextAlign alignment;
    TextWrap wrap;
    TextOrigin origin;
    TextOverflow overflow;
    float paragraphSpacing;
    float alignWidth;
    VerticalTextAlign verticalAlignment;
    float alignHeight;

    // The width lines break at, -1 for the text's own.
    float breakWidth() const
    {
      return sizing == TextSizing::autoWidth ? -1.0f : maxWidth;
    }
    bool breaksLike(const Layout& other) const;
    bool operator==(const Layout& other) const;
  };

  // How a paragraph's lines sit in it.
  struct ParagraphExtent
  {
    float minX;
    float maxX;
    // Its last line's bottom, 0 without lines.
    float bottom;
    uint32_t lineCount;

    void moveDown(float& y, float paragraphSpacing) const;
  };

  struct LineMetrics
  {
    AABB bounds;
    float verticalOffset = 0.0f;
    // The line to put the ellipsis on, -1 for none.
    int ellipsisLine = -1;
    bool isEllipsisLineLast = false;
  };

  static void measureParagraphs(
      const SimpleArray<Paragraph>& paragraphs,
      const SimpleArray<SimpleArray<GlyphLine>>& paragraphLines,
      size_t first,
      size_t end,
      std::vector<ParagraphExtent>& extents);
  static LineMetrics measureLines(
      const std::vector<ParagraphExtent>& extents,
      const SimpleArray<SimpleArray<GlyphLine>>& paragraphLines,
      const Layout& layout);
  // Appends the lines of paragraphs [first, end) to lines, moving y down
  // past them, until the overflow cuts them short.
  void orderLines(
      size_t first,
      size_t end,
      int ellipsisLine,
      bool isEllipsisLineLast,
      float& y,
      std::vector<OrderedLine>& lines);
  // Orders only paragraphs [first, end), which took the place of replaced.
  void patchOrderedLines(
      size_t first,
      size_t end,
      const std::vector<ParagraphExtent>& replaced,
      float y);

  void dropLines();

  SimpleArray<Paragraph> m_paragraphs;
  // Each paragraph's start, then the text's length. Empty when they didn't
  // match the shaper's paragraphs, which turns reshapeParagraphs() off.
  std::vector<uint32_t> m_paragraphStarts;
  SimpleArray<SimpleArray<GlyphLine>> m_paragraphLines;
  std::vector<ParagraphExtent> m_extents;
  // Whether the lines are the last layout's, m_layout what it was asked for.
  bool m_linesBroken = false;
  Layout m_layout = {};
  // Paragraphs shaped since, their lines still to break, and the extents of
  // the paragraphs they replaced.
  size_t m_staleFirst = 0;
  size_t m_staleEnd = 0;
  std::vector<ParagraphExtent> m_staleExtents;
  LineChange m_lineChange;
  std::vector<OrderedLine> m_orderedLines;
  GlyphLookup m_glyphLookup;
  GlyphRun m_ellipsisRun;
  AABB m_bounds;
  float m_verticalOffset = 0.0f;
};
} // namespace rive

#endif
