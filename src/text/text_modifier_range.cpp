#include "rive/text/text.hpp"
#include "rive/text/text_modifier_range.hpp"
#include "rive/text/text_modifier_group.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text/glyph_lookup.hpp"
#include "rive/animation/cubic_interpolator_component.hpp"
#include "rive/core_context.hpp"

using namespace rive;

StatusCode TextModifierRange::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    if (runId() != Core::emptyId)
    {
        auto coreObject = context->resolve(runId());
        if (coreObject == nullptr || !coreObject->is<TextValueRun>())
        {
            return StatusCode::MissingObject;
        }
        m_run = static_cast<TextValueRun*>(coreObject);
    }

    if (parent() != nullptr && parent()->is<TextModifierGroup>())
    {
        parent()->as<TextModifierGroup>()->addModifierRange(this);
        return StatusCode::Ok;
    }

    return StatusCode::MissingObject;
}

void TextModifierRange::addChild(Component* component)
{
    Super::addChild(component);
    if (component->is<CubicInterpolatorComponent>())
    {
        // mark this as the cubic interpolator.
        m_interpolator = component->as<CubicInterpolatorComponent>();
    }
}

void TextModifierRange::clearRangeMap() { m_rangeMapper.clear(); }

void TextModifierRange::computeRange(Span<const Unichar> text,
                                     const SimpleArray<Paragraph>& shape,
                                     const SimpleArray<SimpleArray<GlyphLine>>& lines,
                                     const GlyphLookup& glyphLookup)
{
    // Check if the range mapper is still valid.
    if (!m_rangeMapper.empty())
    {
        return;
    }
    uint32_t start = 0;
    uint32_t end = (uint32_t)text.size();
    if (m_run != nullptr)
    {
        start = m_run->offset();
        end = start + (uint32_t)m_run->text().size();
    }
    switch (units())
    {
        case TextRangeUnits::charactersExcludingSpaces:
            m_rangeMapper.fromCharacters(text, start, end, glyphLookup, true);
            break;
        case TextRangeUnits::words:
            m_rangeMapper.fromWords(text, start, end);
            break;
        case TextRangeUnits::lines:
            m_rangeMapper.fromLines(text, start, end, shape, lines, glyphLookup);
            break;
        default:
            m_rangeMapper.fromCharacters(text, start, end, glyphLookup);
            break;
    }
}

float TextModifierRange::coverageAt(float t)
{
    float c = 0.0f;
    if (m_indexTo >= m_indexFrom)
    {
        if (t < m_indexFrom || t > m_indexTo)
        {
            c = 0.0f;
        }
        else if (t < m_indexFalloffFrom)
        {
            float range = std::max(0.0f, m_indexFalloffFrom - m_indexFrom);
            c = range == 0.0f ? 1.0f : std::max(0.0f, t - m_indexFrom) / range;
            if (m_interpolator != nullptr)
            {
                c = m_interpolator->transform(c);
            }
        }
        else if (t > m_indexFalloffTo)
        {
            float range = std::max(0.0f, m_indexTo - m_indexFalloffTo);
            c = range == 0.0f ? 1.0f : 1.0f - std::min(1.0f, (t - m_indexFalloffTo) / range);
            if (m_interpolator != nullptr)
            {
                c = m_interpolator->transform(c);
            }
        }
        else
        {
            c = 1.0f;
        }
    }
    return c;
}

void TextModifierRange::computeCoverage(Span<float> coverage)
{
    if (m_rangeMapper.empty())
    {
        return;
    }

    // Compute range mapped coverage.
    uint32_t count = m_rangeMapper.unitCount();
    switch (type())
    {
        case TextRangeType::unitIndex:
            m_indexFrom = offsetModifyFrom();
            m_indexTo = offsetModifyTo();
            m_indexFalloffFrom = offsetFalloffFrom();
            m_indexFalloffTo = offsetFalloffTo();
            break;
        case TextRangeType::percentage:
            m_indexFrom = count * offsetModifyFrom();
            m_indexTo = count * offsetModifyTo();
            m_indexFalloffFrom = count * offsetFalloffFrom();
            m_indexFalloffTo = count * offsetFalloffTo();
            break;
    }

    TextRangeMode rangeMode = mode();
    for (uint32_t unitIndex = 0; unitIndex < count; unitIndex++)
    {
        uint32_t unitLength = (uint32_t)m_rangeMapper.unitLength(unitIndex);
        uint32_t characterIndex = m_rangeMapper.unitCharacterIndex(unitIndex);
        float c = strength() * coverageAt(unitIndex + 0.5f);
        for (uint32_t i = 0; i < unitLength; i++)
        {
            assert((characterIndex + i) < (uint32_t)coverage.size());
            float current = coverage[characterIndex + i];
            switch (rangeMode)
            {
                case TextRangeMode::add:
                    current += c;
                    break;
                case TextRangeMode::subtract:
                    current -= c;
                    break;
                case TextRangeMode::max:
                    current = std::max(current, c);
                    break;
                case TextRangeMode::min:
                    current = std::min(current, c);
                    break;
                case TextRangeMode::multiply:
                    current *= c;
                    break;
                case TextRangeMode::difference:
                    current = std::abs(current - c);
                    break;
            }
            coverage[characterIndex + i] =
                clamp() ? std::max(0.0f, std::min(1.0f, current)) : current;
        }

        // Set space between units coverage to 0.
        if (unitIndex + 1 < m_rangeMapper.unitCharacterIndexCount())
        {
            uint32_t nextCharacterIndex = m_rangeMapper.unitCharacterIndex(unitIndex + 1);
            for (uint32_t i = characterIndex + unitLength; i < nextCharacterIndex; i++)
            {
                coverage[i] = 0;
            }
        }
    }
}

void TextModifierRange::modifyFromChanged() { parent()->as<TextModifierGroup>()->rangeChanged(); }
void TextModifierRange::modifyToChanged() { parent()->as<TextModifierGroup>()->rangeChanged(); }
void TextModifierRange::strengthChanged() { parent()->as<TextModifierGroup>()->rangeChanged(); }
void TextModifierRange::unitsValueChanged()
{
    parent()->as<TextModifierGroup>()->rangeTypeChanged();
}
void TextModifierRange::typeValueChanged() { parent()->as<TextModifierGroup>()->rangeChanged(); }
void TextModifierRange::modeValueChanged() { parent()->as<TextModifierGroup>()->rangeChanged(); }
void TextModifierRange::clampChanged() { parent()->as<TextModifierGroup>()->rangeChanged(); }
void TextModifierRange::falloffFromChanged() { parent()->as<TextModifierGroup>()->rangeChanged(); }
void TextModifierRange::falloffToChanged() { parent()->as<TextModifierGroup>()->rangeChanged(); }
void TextModifierRange::offsetChanged() { parent()->as<TextModifierGroup>()->rangeChanged(); }

bool TextModifierRange::needsShape() const { return units() == TextRangeUnits::lines; }

void RangeMapper::clear()
{
    m_unitCharacterIndices.clear();
    m_unitLengths.clear();
}

float RangeMapper::unitToCharacterRange(float word) const
{
    if (m_unitCharacterIndices.empty())
    {
        return 0.0f;
    }
    float clampedUnit = std::min(std::max(word, 0.0f), (float)(m_unitCharacterIndices.size() - 1));
    size_t intUnit = (size_t)clampedUnit;
    float characters = (float)m_unitCharacterIndices[intUnit];
    if (intUnit < m_unitLengths.size())
    {
        float fraction = clampedUnit - intUnit;
        characters += m_unitLengths[intUnit] * fraction;
    }
    return characters;
}

void RangeMapper::addRange(uint32_t indexFrom,
                           uint32_t indexTo,
                           uint32_t startOffset,
                           uint32_t endOffset)
{
    if (indexTo > startOffset && endOffset > indexFrom)
    {
        uint32_t actualStart = std::max(startOffset, indexFrom);
        uint32_t actualEnd = std::min(endOffset, indexTo);
        if (actualEnd > actualStart)
        {
            m_unitCharacterIndices.push_back(actualStart);
            m_unitLengths.push_back(actualEnd - actualStart);
        }
    }
}

void RangeMapper::fromWords(Span<const Unichar> text, uint32_t start, uint32_t end)
{
    if (text.empty())
    {
        return;
    }

    bool wantWhiteSpace = false;
    uint32_t characterCount = 0;
    uint32_t index = 0;
    uint32_t indexFrom = 0;
    for (Unichar unit : text)
    {
        if (wantWhiteSpace == isWhiteSpace(unit))
        {
            if (!wantWhiteSpace)
            {
                indexFrom = index;
            }
            else
            {
                addRange(indexFrom, indexFrom + characterCount, start, end);

                characterCount = 0;
            }
            wantWhiteSpace = !wantWhiteSpace;
        }
        if (wantWhiteSpace)
        {
            characterCount++;
        }
        index++;
    }
    if (characterCount > 0)
    {
        addRange(indexFrom, indexFrom + characterCount, start, end);
    }
    m_unitCharacterIndices.push_back(end);
}

void RangeMapper::fromCharacters(Span<const Unichar> text,
                                 uint32_t start,
                                 uint32_t end,
                                 const GlyphLookup& glyphLookup,
                                 bool withoutSpaces)
{
    if (text.empty())
    {
        return;
    }
    for (uint32_t i = start; i < end;)
    {
        Unichar unit = text[i];
        if (withoutSpaces && isWhiteSpace(unit))
        {
            i++;
            continue;
        }
        // Don't break ligated glyphs.
        uint32_t codePoints = glyphLookup.count(i);
        m_unitCharacterIndices.push_back(i);
        m_unitLengths.push_back(codePoints);
        i += codePoints;
    }

    m_unitCharacterIndices.push_back(end);
}

void RangeMapper::fromLines(Span<const Unichar> text,
                            uint32_t start,
                            uint32_t end,
                            const SimpleArray<Paragraph>& shape,
                            const SimpleArray<SimpleArray<GlyphLine>>& lines,
                            const GlyphLookup& glyphLookup)
{
    if (text.empty())
    {
        return;
    }
    uint32_t paragraphIndex = 0;
    for (const SimpleArray<GlyphLine>& paragraphLines : lines)
    {
        const Paragraph& paragraph = shape[paragraphIndex++];
        const SimpleArray<GlyphRun>& glyphRuns = paragraph.runs;
        for (const GlyphLine& line : paragraphLines)
        {
            const GlyphRun& rf = glyphRuns[line.startRunIndex];
            uint32_t indexFrom = rf.textIndices[line.startGlyphIndex];

            const GlyphRun& rt = glyphRuns[line.endRunIndex];
            uint32_t endGlyphIndex = line.endGlyphIndex == 0 ? 0 : line.endGlyphIndex - 1;
            uint32_t indexTo = rt.textIndices[endGlyphIndex];
            indexTo += glyphLookup.count(indexTo);

            addRange(indexFrom, indexTo, start, end);
        }
    }
    m_unitCharacterIndices.push_back(end);
}
