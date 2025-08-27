#ifndef _RIVE_FULLY_SHAPED_TEXT_HPP_
#define _RIVE_FULLY_SHAPED_TEXT_HPP_

#include "rive/span.hpp"
#include "rive/text/utf.hpp"
#include "rive/text_engine.hpp"
#include "rive/text/glyph_lookup.hpp"

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

    void shape(Span<Unichar> text,
               Span<TextRun> runs,
               TextSizing sizing,
               float maxWidth,
               float maxHeight,
               TextAlign alignment,
               TextWrap wrap,
               TextOrigin origin,
               TextOverflow overflow,
               float paragraphSpacing);

private:
    SimpleArray<Paragraph> m_paragraphs;
    SimpleArray<SimpleArray<GlyphLine>> m_paragraphLines;
    std::vector<OrderedLine> m_orderedLines;
    GlyphLookup m_glyphLookup;
    GlyphRun m_ellipsisRun;
    AABB m_bounds;
};
} // namespace rive

#endif