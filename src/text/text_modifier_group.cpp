#include "rive/text/text.hpp"
#include "rive/text/text_modifier_group.hpp"
#include "rive/text/text_shape_modifier.hpp"
#include "rive/text/text_modifier_range.hpp"
#include "rive/text/glyph_lookup.hpp"
#include "rive/text/text_style.hpp"
#include "rive/artboard.hpp"
#include <limits>

using namespace rive;

StatusCode TextModifierGroup::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    if (parent() != nullptr && parent()->is<Text>())
    {
        parent()->as<Text>()->addModifierGroup(this);
        return StatusCode::Ok;
    }

    return StatusCode::MissingObject;
}

void TextModifierGroup::addModifierRange(TextModifierRange* range) { m_ranges.push_back(range); }

void TextModifierGroup::addModifier(TextModifier* modifier)
{
    m_modifiers.push_back(modifier);
    if (modifier->is<TextShapeModifier>())
    {
        m_shapeModifiers.push_back(modifier->as<TextShapeModifier>());
    }
}

void TextModifierGroup::rangeTypeChanged()
{
    parent()->as<Text>()->modifierShapeDirty();
    addDirt(ComponentDirt::TextCoverage);
}

void TextModifierGroup::shapeModifierChanged() { parent()->as<Text>()->markShapeDirty(); }

void TextModifierGroup::rangeChanged()
{
    /// Marking shape dirty should only be done if this modifer group changes
    /// shaping properties (for now we're just testing and we're hardcoding a
    /// shaping change).
    if (!m_shapeModifiers.empty())
    {
        parent()->as<Text>()->modifierShapeDirty();
    }
    else
    {
        parent()->as<Text>()->markPaintDirty();
    }
    addDirt(ComponentDirt::TextCoverage);
}

/// Clear any cached selector range maps so they can be recomputed after next
/// shaping.
void TextModifierGroup::clearRangeMaps()
{
    for (TextModifierRange* range : m_ranges)
    {
        range->clearRangeMap();
    }
    addDirt(ComponentDirt::TextCoverage);
}

void TextModifierGroup::computeRangeMap(Span<const Unichar> text,
                                        const SimpleArray<Paragraph>& shape,
                                        const SimpleArray<SimpleArray<GlyphLine>>& lines,
                                        const GlyphLookup& glyphLookup)
{
    for (TextModifierRange* range : m_ranges)
    {
        range->computeRange(text, shape, lines, glyphLookup);
    }
}

void TextModifierGroup::computeCoverage(uint32_t textSize)
{
    if (!hasDirt(ComponentDirt::TextCoverage))
    {
        return;
    }

    // Because we're not dependent on anything we need to reset our dirt
    // ourselves. We're not in the DAG so we'll never get reset.
    m_Dirt = ComponentDirt::None;

    // When the text re-shapes, we udpate our coverage values.
    m_coverage.resize(textSize);
    std::fill(m_coverage.begin(), m_coverage.end(), 0);
    for (TextModifierRange* range : m_ranges)
    {
        range->computeCoverage(m_coverage);
    }
}

float TextModifierGroup::glyphCoverage(uint32_t textIndex, uint32_t codePointCount)
{
    assert(codePointCount >= 1);
    float c = coverage(textIndex);

    for (uint32_t i = 1; i < codePointCount; i++)
    {
        c += coverage(textIndex + i);
    }
    return c /= (float)codePointCount;
}

void TextModifierGroup::transform(float amount, Mat2D& ctm)
{
    if (amount == 0.0f || !modifiesTransform())
    {
        return;
    }

    float actualRotation = modifiesRotation() ? rotation() * amount : 0.0f;
    Mat2D transform = actualRotation != 0.0f ? Mat2D::fromRotation(actualRotation) : Mat2D();
    if (modifiesTranslation())
    {
        transform[4] = x() * amount;
        transform[5] = y() * amount;
    }
    if (modifiesScale())
    {
        float iamount = 1.0f - amount;
        transform.scaleByValues(iamount + scaleX() * amount, iamount + scaleY() * amount);
    }
    bool doesModifyOrigin = modifiesOrigin();
    if (doesModifyOrigin)
    {
        ctm[4] += originX();
        ctm[5] += originY();
    }
    ctm = transform * ctm;
    if (doesModifyOrigin)
    {
        ctm[4] -= originX();
        ctm[5] -= originY();
    }
}

float TextModifierGroup::computeOpacity(float current, float t) const
{
    if ((modifierFlags() & (uint32_t)TextModifierFlags::invertOpacity) != 0)
    {
        return current * (1.0f - t) + opacity() * t;
    }
    else
    {
        return current * opacity() * t;
    }
}

void TextModifierGroup::modifierFlagsChanged() { parent()->as<Text>()->markPaintDirty(); }
void TextModifierGroup::originXChanged() { parent()->as<Text>()->markPaintDirty(); }
void TextModifierGroup::originYChanged() { parent()->as<Text>()->markPaintDirty(); }
void TextModifierGroup::opacityChanged() { parent()->as<Text>()->markPaintDirty(); }
void TextModifierGroup::xChanged() { parent()->as<Text>()->markPaintDirty(); }
void TextModifierGroup::yChanged() { parent()->as<Text>()->markPaintDirty(); }
void TextModifierGroup::rotationChanged() { parent()->as<Text>()->markPaintDirty(); }
void TextModifierGroup::scaleXChanged() { parent()->as<Text>()->markPaintDirty(); }
void TextModifierGroup::scaleYChanged() { parent()->as<Text>()->markPaintDirty(); }

static TextRun copyRun(const TextRun& source, uint32_t unicharCount)
{
    return {
        source.font,
        source.size,
        source.lineHeight,
        source.letterSpacing,
        unicharCount,
        source.script,
        source.styleId,
        source.dir,
    };
}

TextRun TextModifierGroup::modifyShape(const Text& text, TextRun run, float strength)
{
    const TextStyle* style = text.styleFromShaperId(run.styleId);
    if (style == nullptr || style->font() == nullptr)
    {
        return run;
    }

    Font* font = style->font().get();

    std::unordered_map<uint32_t, float> variations;
    float fontSize = run.size;
    for (TextShapeModifier* shapeModifier : m_shapeModifiers)
    {
        fontSize = shapeModifier->modify(font, variations, fontSize, strength);
    }

    if (!variations.empty())
    {
        m_variationCoords.clear();
        for (auto itr = variations.begin(); itr != variations.end(); itr++)
        {
            m_variationCoords.push_back({itr->first, itr->second});
        }
        m_variableFont = font->makeAtCoords(m_variationCoords);
        return {
            m_variableFont,
            run.size,
            run.lineHeight,
            run.letterSpacing,
            run.unicharCount,
            run.script,
            run.styleId,
            run.dir,
        };
    }
    else
    {
        m_variableFont = nullptr;
    }
    return run;
}

void TextModifierGroup::applyShapeModifiers(const Text& text, StyledText& styledText)
{
    if (m_shapeModifiers.empty())
    {
        return;
    }
    m_nextTextRuns.clear();
    m_nextTextRuns.reserve(styledText.runs().size());

    uint32_t index = 0;
    float lastCoverage = std::numeric_limits<float>::max();
    uint32_t extractRunIndex = 0;

    for (const TextRun& run : styledText.runs())
    {
        // Split the run into sub-runs as necessary based on equal coverage
        // values.
        uint32_t end = index + run.unicharCount;

        while (index < end)
        {
            float coverage = m_coverage[index];
            if (coverage != lastCoverage)
            {
                if (index - extractRunIndex != 0)
                {
                    // Add new run from extractRunStart to index (exclusive)
                    if (lastCoverage == 0.0f)
                    {
                        m_nextTextRuns.push_back(copyRun(run, index - extractRunIndex));
                    }
                    else
                    {
                        m_nextTextRuns.push_back(
                            modifyShape(text, copyRun(run, index - extractRunIndex), lastCoverage));
                    }
                }
                lastCoverage = coverage;
                extractRunIndex = index;
            }
            index++;
        }

        assert(extractRunIndex != end);
        // Add new run from extractRunStart to index (exclusive)
        if (lastCoverage == 0.0f)
        {
            m_nextTextRuns.push_back(copyRun(run, end - extractRunIndex));
        }
        else
        {
            m_nextTextRuns.push_back(
                modifyShape(text, copyRun(run, end - extractRunIndex), lastCoverage));
        }
        extractRunIndex = end;
    }
    styledText.swapRuns(m_nextTextRuns);
    // return nextTextRuns;
}

bool TextModifierGroup::needsShape() const
{
    if (!m_shapeModifiers.empty())
    {
        return true;
    }
    for (const TextModifierRange* range : m_ranges)
    {
        if (range->needsShape())
        {
            return true;
        }
    }
    return false;
}