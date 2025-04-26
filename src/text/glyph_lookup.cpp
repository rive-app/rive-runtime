#include "rive/text/glyph_lookup.hpp"
#include "rive/text_engine.hpp"

using namespace rive;

void GlyphLookup::compute(Span<const Unichar> text,
                          const SimpleArray<Paragraph>& shape)
{
    size_t codeUnitCount = text.size();
    m_glyphIndices.resize(codeUnitCount + 1);
    // Build a mapping of codePoints to glyphs indices.
    uint32_t glyphIndex = 0;
    uint32_t lastTextIndex = 0;
    for (const Paragraph& paragraph : shape)
    {
        for (const GlyphRun& run : paragraph.runs)
        {
            for (size_t i = 0; i < run.glyphs.size(); i++)
            {
                uint32_t textIndex = run.textIndices[i];
                for (uint32_t j = lastTextIndex; j < textIndex; j++)
                {
                    assert(glyphIndex != 0);
                    m_glyphIndices[j] = glyphIndex - 1;
                }
                lastTextIndex = textIndex;
                glyphIndex++;
            }
        }
    }
    for (size_t i = lastTextIndex; i < codeUnitCount; i++)
    {
        m_glyphIndices[i] = glyphIndex - 1;
    }

    // Store a fake unreachable glyph at the end to allow selecting the last
    // one.
    m_glyphIndices[codeUnitCount] =
        codeUnitCount == 0 ? 0 : m_glyphIndices[codeUnitCount - 1] + 1;
}

uint32_t GlyphLookup::count(uint32_t index) const
{
    assert(index < (uint32_t)m_glyphIndices.size());

    uint32_t value = m_glyphIndices[index];
    uint32_t count = 1;
    uint32_t size = (uint32_t)m_glyphIndices.size();
    while (++index < size && m_glyphIndices[index] == value)
    {
        count++;
    }
    return count;
}

float GlyphLookup::advanceFactor(int32_t codePointIndex, bool inv) const
{
    if (codePointIndex < 0 || codePointIndex >= (int32_t)m_glyphIndices.size())
    {
        return 0.0f;
    }
    uint32_t glyphIndex = m_glyphIndices[codePointIndex];
    int32_t start = codePointIndex;
    while (start > 0)
    {
        if (m_glyphIndices[start - 1] != glyphIndex)
        {
            break;
        }
        start--;
    }
    int32_t end = codePointIndex;
    while (end < m_glyphIndices.size() - 1)
    {
        if (m_glyphIndices[end + 1] != glyphIndex)
        {
            break;
        }
        end++;
    }

    float f = (codePointIndex - start) / (float)(end - start + 1);
    if (inv)
    {
        return 1.0f - f;
    }
    return f;
}