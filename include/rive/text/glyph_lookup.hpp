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
    size_t size() const { return m_glyphIndices.size(); }

    // Returns the glyph index from the computed shape given the codePointIndex
    // in the original text.
    uint32_t operator[](uint32_t codePointIndex) const
    {
        assert(codePointIndex >= 0 &&
               codePointIndex < (uint32_t)m_glyphIndices.size());
        return m_glyphIndices[codePointIndex];
    }

    uint32_t lastCodeUnitIndex() const
    {
        return m_glyphIndices.size() == 0
                   ? 0
                   : (uint32_t)(m_glyphIndices.size() - 1);
    }

    bool empty() const { return m_glyphIndices.empty(); }
    void clear() { m_glyphIndices.clear(); }

    // How far this codePoint index is within the glyph.
    float advanceFactor(int32_t codePointIndex, bool inv) const;
};
} // namespace rive

#endif