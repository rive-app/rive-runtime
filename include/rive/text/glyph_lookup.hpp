#ifndef _RIVE_TEXT_GLYPH_LOOKUP_HPP_
#define _RIVE_TEXT_GLYPH_LOOKUP_HPP_
#include "rive/text_engine.hpp"
#include <stdint.h>
#include <vector>

namespace rive
{

/// Stores the glyphId/Index representing the unicode point at i.
class GlyphLookup
{
public:
    void compute(Span<const Unichar> text, const SimpleArray<Paragraph>& shape);

private:
    std::vector<uint32_t> m_glyphIndices;

public:
    uint32_t count(uint32_t index) const;

    bool empty() const { return m_glyphIndices.empty(); }
    void clear() { m_glyphIndices.clear(); }
};
} // namespace rive

#endif