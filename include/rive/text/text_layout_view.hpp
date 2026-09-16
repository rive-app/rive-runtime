#ifndef _RIVE_TEXT_LAYOUT_VIEW_HPP_
#define _RIVE_TEXT_LAYOUT_VIEW_HPP_

#include "rive/text/glyph_lookup.hpp"

namespace rive
{
// Borrowed layout data for selection. The owner must keep the shaped text
// alive and unchanged for the duration of a cursor operation. textLength is
// the last selectable source offset: inputs exclude their trailing sentinel,
// while regular Text can select through the end of its actual source.
class TextLayoutView
{
public:
    TextLayoutView(const SimpleArray<Paragraph>& paragraphs,
                   const SimpleArray<SimpleArray<GlyphLine>>& paragraphLines,
                   const std::vector<OrderedLine>& orderedLines,
                   const GlyphLookup& glyphLookup,
                   uint32_t textLength) :
        m_paragraphs(paragraphs),
        m_paragraphLines(paragraphLines),
        m_orderedLines(orderedLines),
        m_glyphLookup(glyphLookup),
        m_textLength(textLength)
    {}

    const SimpleArray<Paragraph>& paragraphs() const { return m_paragraphs; }
    const SimpleArray<SimpleArray<GlyphLine>>& paragraphLines() const
    {
        return m_paragraphLines;
    }
    const std::vector<OrderedLine>& orderedLines() const
    {
        return m_orderedLines;
    }
    const GlyphLookup& glyphLookup() const { return m_glyphLookup; }
    uint32_t textLength() const { return m_textLength; }

private:
    const SimpleArray<Paragraph>& m_paragraphs;
    const SimpleArray<SimpleArray<GlyphLine>>& m_paragraphLines;
    const std::vector<OrderedLine>& m_orderedLines;
    const GlyphLookup& m_glyphLookup;
    uint32_t m_textLength;
};
} // namespace rive
#endif
