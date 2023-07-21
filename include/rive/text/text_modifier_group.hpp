#ifndef _RIVE_TEXT_MODIFIER_GROUP_HPP_
#define _RIVE_TEXT_MODIFIER_GROUP_HPP_
#include "rive/generated/text/text_modifier_group_base.hpp"
#include "rive/text/text_modifier_flags.hpp"
#include "rive/text_engine.hpp"

#include <vector>

namespace rive
{
class TextModifierRange;
class TextModifier;
class TextShapeModifier;
class GlyphLookup;
class Text;
class StyledText;
class TextModifierGroup : public TextModifierGroupBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;

    void addModifierRange(TextModifierRange* range);
    void addModifier(TextModifier* modifier);
    void rangeChanged();
    void rangeTypeChanged();
    void clearRangeMaps();
    void computeRangeMap(Span<const Unichar> text,
                         const rive::SimpleArray<rive::Paragraph>& shape,
                         const SimpleArray<SimpleArray<GlyphLine>>& lines,
                         const GlyphLookup& glyphLookup);
    void computeCoverage(uint32_t textSize);
    float glyphCoverage(uint32_t textIndex, uint32_t codePointCount);
    float coverage(uint32_t textIndex)
    {
        assert(textIndex < m_coverage.size());
        return m_coverage[textIndex];
    }
    void transform(float amount, Mat2D& ctm);
    TextRun modifyShape(const Text& text, TextRun run, float strength);
    void applyShapeModifiers(const Text& text, StyledText& styledText);

    bool modifiesTransform() const
    {
        return (modifierFlags() &
                (uint32_t)(TextModifierFlags::modifyTranslation |
                           TextModifierFlags::modifyRotation | TextModifierFlags::modifyScale |
                           TextModifierFlags::modifyOrigin)) != 0;
    }

    bool modifiesOpacity() const
    {
        return (modifierFlags() & (uint32_t)TextModifierFlags::modifyOpacity) != 0;
    }

    bool modifiesRotation() const
    {
        return (modifierFlags() & (uint32_t)TextModifierFlags::modifyRotation) != 0;
    }

    bool modifiesTranslation() const
    {
        return (modifierFlags() & (uint32_t)TextModifierFlags::modifyTranslation) != 0;
    }

    bool modifiesScale() const
    {
        return (modifierFlags() & (uint32_t)TextModifierFlags::modifyScale) != 0;
    }

    bool modifiesOrigin() const
    {
        return (modifierFlags() & (uint32_t)TextModifierFlags::modifyOrigin) != 0;
    }

    float computeOpacity(float current, float t) const;
    bool needsShape() const;

#ifdef TESTING
    const std::vector<TextModifierRange*>& ranges() const { return m_ranges; }
    const std::vector<TextModifier*>& modifiers() const { return m_modifiers; }
#endif

protected:
    void modifierFlagsChanged() override;
    void originXChanged() override;
    void originYChanged() override;
    void opacityChanged() override;
    void xChanged() override;
    void yChanged() override;
    void rotationChanged() override;
    void scaleXChanged() override;
    void scaleYChanged() override;

private:
    std::vector<TextModifierRange*> m_ranges;
    std::vector<TextModifier*> m_modifiers;
    std::vector<TextShapeModifier*> m_shapeModifiers;
    std::vector<float> m_coverage;
    rcp<Font> m_variableFont;
    std::vector<Font::Coord> m_variationCoords;
    std::vector<TextRun> m_nextTextRuns;
};
} // namespace rive

#endif