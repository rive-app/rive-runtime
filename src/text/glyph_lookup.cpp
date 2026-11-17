#include "rive/text/glyph_lookup.hpp"
#include "rive/text_engine.hpp"

using namespace rive;

// Maps code points [lastTextIndex, end) to the glyphs of paragraphs, which
// are numbered from glyphIndex. Leaves glyphIndex one past the last of them.
void GlyphLookup::map(
    Span<const Paragraph> paragraphs,
    uint32_t end,
    uint32_t& glyphIndex,
    uint32_t lastTextIndex)
{
  for (const Paragraph& paragraph : paragraphs)
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
  for (uint32_t i = lastTextIndex; i < end; i++)
  {
    m_glyphIndices[i] = glyphIndex - 1;
  }
}

void GlyphLookup::compute(
    Span<const Unichar> text,
    const SimpleArray<Paragraph>& shape)
{
  uint32_t codePointCount = (uint32_t)text.size();
  m_glyphIndices.resize(codePointCount + 1);
  uint32_t glyphIndex = 0;
  map(Span<const Paragraph>(shape.data(), shape.size()),
      codePointCount,
      glyphIndex,
      0);

  // Store a fake unreachable glyph at the end to allow selecting the last
  // one.
  m_glyphIndices[codePointCount] = codePointCount == 0
                                       ? 0
                                       : m_glyphIndices[codePointCount - 1] + 1;
}

void GlyphLookup::replace(
    uint32_t start,
    uint32_t oldLength,
    uint32_t newLength,
    Span<const Paragraph> paragraphs)
{
  uint32_t glyphIndex = start == 0 ? 0 : m_glyphIndices[start - 1] + 1;
  uint32_t oldGlyphEnd = oldLength == 0
                             ? glyphIndex
                             : m_glyphIndices[start + oldLength - 1] + 1;
  auto at = m_glyphIndices.begin() + start;
  if (newLength > oldLength)
  {
    m_glyphIndices.insert(at + oldLength, newLength - oldLength, 0);
  }
  else
  {
    m_glyphIndices.erase(at + newLength, at + oldLength);
  }
  map(paragraphs, start + newLength, glyphIndex, start);

  // Everything after is as many glyphs further along as the paragraphs
  // gained.
  uint32_t gained = glyphIndex - oldGlyphEnd;
  for (size_t i = start + newLength; i < m_glyphIndices.size(); i++)
  {
    m_glyphIndices[i] += gained;
  }
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

uint32_t GlyphLookup::glyphStart(uint32_t index) const
{
  if (index == 0 || index >= (uint32_t)m_glyphIndices.size())
  {
    return index;
  }
  uint32_t value = m_glyphIndices[index];
  while (index > 0 && m_glyphIndices[index - 1] == value)
  {
    index--;
  }
  return index;
}

bool GlyphLookup::isGlyphBoundary(uint32_t index) const
{
  if (index == 0 || index >= (uint32_t)m_glyphIndices.size())
  {
    return true;
  }
  return m_glyphIndices[index] != m_glyphIndices[index - 1];
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