#ifndef _RIVE_TEXT_MODIFIER_RANGE_HPP_
#define _RIVE_TEXT_MODIFIER_RANGE_HPP_
#include "rive/generated/text/text_modifier_range_base.hpp"
#include "rive/text_engine.hpp"
#include <vector>

namespace rive
{
enum class TextRangeUnits : uint8_t
{
    characters,
    charactersExcludingSpaces,
    words,
    lines
};

enum class TextRangeMode : uint8_t
{
    add,
    subtract,
    multiply,
    min,
    max,
    difference
};

enum class TextRangeType : uint8_t
{
    percentage,
    unitIndex
};

enum class TextRangeInterpolator : uint8_t
{
    linear,
    cubic
};

class GlyphLookup;
class TextValueRun;
class RangeMapper
{
public:
    uint32_t unitCount() { return (uint32_t)m_unitLengths.size(); }
    uint32_t unitCharacterIndexCount() { return (uint32_t)m_unitCharacterIndices.size(); }

    void clear();
    bool empty() { return m_unitLengths.empty(); }

    /// Compute ranges of words.
    void fromWords(Span<const Unichar> text, uint32_t start, uint32_t end);

    /// Compute ranges of characters in text.
    void fromCharacters(Span<const Unichar> text,
                        uint32_t start,
                        uint32_t end,
                        const GlyphLookup& glyphLookup,
                        bool withoutSpaces = false);

    /// Compute ranges of lines.
    void fromLines(Span<const Unichar> text,
                   uint32_t start,
                   uint32_t end,
                   const SimpleArray<Paragraph>& shape,
                   const SimpleArray<SimpleArray<GlyphLine>>& lines,
                   const GlyphLookup& glyphLookup);

    float unitToCharacterRange(float word) const;

    uint32_t unitCharacterIndex(uint32_t at) const
    {
        assert(at < m_unitCharacterIndices.size());
        return m_unitCharacterIndices[at];
    }

    uint32_t unitLength(uint32_t at) const
    {
        assert(at < m_unitLengths.size());
        return m_unitLengths[at];
    }

    // Add (some of) unit at indexFrom to indexTo where it falls within the start/end offset.
    void addRange(uint32_t indexFrom, uint32_t indexTo, uint32_t startOffset, uint32_t endOffset);

private:
    /// Each item in this list represents the index (in unicode codepoints) of
    /// the selectable element. Always has length 1+unitLengths.length as it's
    /// expected to always include the final index with 0 length.
    std::vector<uint32_t> m_unitCharacterIndices;

    /// Each item in this list represents the length of the matching element at
    /// the same index in the _unitIndices list.
    std::vector<uint32_t> m_unitLengths;
};

class CubicInterpolatorComponent;
class TextModifierRange : public TextModifierRangeBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;

    void clearRangeMap();
    void computeRange(Span<const Unichar> text,
                      const SimpleArray<Paragraph>& shape,
                      const SimpleArray<SimpleArray<GlyphLine>>& lines,
                      const GlyphLookup& glyphLookup);
    void computeCoverage(Span<float> coverage);

    TextRangeUnits units() const { return (TextRangeUnits)unitsValue(); }
    TextRangeType type() const { return (TextRangeType)typeValue(); }
    TextRangeMode mode() const { return (TextRangeMode)modeValue(); }
    void addChild(Component* component) override;
    bool needsShape() const;

#ifdef TESTING
    CubicInterpolatorComponent* interpolator() const { return m_interpolator; }
#endif

protected:
    void modifyFromChanged() override;
    void modifyToChanged() override;
    void strengthChanged() override;
    void unitsValueChanged() override;
    void typeValueChanged() override;
    void modeValueChanged() override;
    void clampChanged() override;
    void falloffFromChanged() override;
    void falloffToChanged() override;
    void offsetChanged() override;

private:
    RangeMapper m_rangeMapper;
    // Cache indices.
    float m_indexFrom = 0.0f;
    float m_indexTo = 0.0f;
    float m_indexFalloffFrom = 0.0f;
    float m_indexFalloffTo = 0.0f;
    CubicInterpolatorComponent* m_interpolator = nullptr;
    TextValueRun* m_run = nullptr;

    float offsetModifyFrom() const { return modifyFrom() + offset(); }
    float offsetModifyTo() const { return modifyTo() + offset(); }
    float offsetFalloffFrom() const { return falloffFrom() + offset(); }
    float offsetFalloffTo() const { return falloffTo() + offset(); }

    float coverageAt(float t);

public:
#ifdef TESTING
    TextValueRun* run() const { return m_run; }
#endif
};
} // namespace rive

#endif