#ifndef _RIVE_TEXT_CORE_HPP_
#define _RIVE_TEXT_CORE_HPP_
#include "rive/generated/text/text_base.hpp"
#include "rive/math/aabb.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text_engine.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/simple_array.hpp"
#include "rive/text/glyph_lookup.hpp"
#include "rive/text/text_interface.hpp"
#include "rive/data_bind/data_bind_list_item_consumer.hpp"
#include "rive/viewmodel/symbol_type.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/property_symbol_dependent.hpp"
#include "rive/dirtyable.hpp"

#include <unordered_map>
#include <vector>

namespace rive
{

class TextModifierGroup;

class StyledText
{
private:
    /// Represents the unicode characters making up the entire text string
    /// displayed. Only valid after update.
    std::vector<Unichar> m_value;
    std::vector<TextRun> m_runs;

public:
    bool empty() const;
    void clear();
    void append(rcp<Font> font,
                float size,
                float lineHeight,
                float letterSpacing,
                const std::string& text,
                uint16_t styleId);
    const std::vector<Unichar>& unichars() const { return m_value; }
    const std::vector<TextRun>& runs() const { return m_runs; }

    void swapRuns(std::vector<TextRun>& otherRuns) { m_runs.swap(otherRuns); }
};

struct TextBoundsInfo
{
    float minY;
    float maxWidth;
    float totalHeight;
    int ellipsisLine;
    bool isEllipsisLineLast;
};

enum class LineIter : uint8_t
{
    drawLine,
    skipThisLine,
    yOutOfBounds
};

class Text;

#ifdef WITH_RIVE_TEXT
class TextValueRunListener;

class TextValueRunProperty : public PropertySymbolDependentSingle
{
public:
    TextValueRunProperty(Core* textValueRun,
                         TextValueRunListener* textValueRunListener,
                         ViewModelInstanceValue* instanceValue,
                         uint16_t propertyKey,
                         SymbolType symbolType);

    void writeValue() override;

private:
    SymbolType m_symbolType = SymbolType::none;
};

class TextValueRunListener : public CoreObjectListener
{
public:
    TextValueRunListener(TextValueRun* textValueRun,
                         rcp<ViewModelInstance> instance,
                         Text* text);

    void markDirty() override;
    Text* text() { return m_text; }
    TextValueRun* textValueRun()
    {
        if (m_core)
        {
            return m_core->as<TextValueRun>();
        }
        return nullptr;
    }

protected:
    void createProperties() override;

private:
    Text* m_text = nullptr;
    void createPropertyListener(SymbolType symbolType);
    TextValueRunProperty* createSinglePropertyListener(SymbolType symbolType);
};
#endif

class TextStylePaint;
class Text : public TextBase,
             public TextInterface,
             public DataBindListItemConsumer
{
public:
    // Implements TextInterface
    ~Text();
    void markShapeDirty() override;
    void markPaintDirty() override;

    void draw(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    void addRun(TextValueRun* run);
    void addModifierGroup(TextModifierGroup* group);
    void markShapeDirty(bool sendToLayout);
    void modifierShapeDirty();

    void update(ComponentDirt value) override;
    void onDirty(ComponentDirt value) override;
    Mat2D m_transform;
    Mat2D m_shapeWorldTransform;

    const Mat2D& shapeWorldTransform() const { return m_shapeWorldTransform; }
    TextSizing sizing() const { return (TextSizing)sizingValue(); }
    TextSizing effectiveSizing() const;
    TextOverflow overflow() const { return (TextOverflow)overflowValue(); }
    TextOrigin textOrigin() const { return (TextOrigin)originValue(); }
    TextWrap wrap() const { return (TextWrap)wrapValue(); }
    VerticalTextAlign verticalAlign() const
    {
        return (VerticalTextAlign)verticalAlignValue();
    }
    TextAlign align() const;
    void overflow(TextOverflow value) { return overflowValue((uint32_t)value); }
    void buildRenderStyles();
    const TextStylePaint* styleFromShaperId(uint16_t id) const;
    bool modifierRangesNeedShape() const;
    AABB localBounds() const override;
    AABB constraintBounds() const override { return localBounds(); }
    void originXChanged() override;
    void originYChanged() override;

    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
    void controlSize(Vec2D size,
                     LayoutScaleType widthScaleType,
                     LayoutScaleType heightScaleType,
                     LayoutDirection direction) override;
    float effectiveWidth()
    {
        return std::isnan(m_layoutWidth) ? width() : m_layoutWidth;
    }
    float effectiveHeight()
    {
        return std::isnan(m_layoutHeight) ? height() : m_layoutHeight;
    }
    float computedWidth() override { return localBounds().width(); };
    float computedHeight() override { return localBounds().height(); };
    void updateList(std::vector<rcp<ViewModelInstanceListItem>>* list) override;
#ifdef WITH_RIVE_TEXT
    const std::vector<TextValueRun*>& runs() const { return m_allRuns; }
    static SimpleArray<SimpleArray<GlyphLine>> BreakLines(
        const SimpleArray<Paragraph>& paragraphs,
        float width,
        TextAlign align,
        TextWrap wrap);
    const std::vector<TextStylePaint*>& textStylePaints()
    {
        return m_textStylePaints;
    }
#endif

    bool haveModifiers() const
    {
#ifdef WITH_RIVE_TEXT
        return !m_modifierGroups.empty();
#else
        return false;
#endif
    }
#ifdef TESTING
    const std::vector<OrderedLine>& orderedLines() const
    {
        return m_orderedLines;
    }
    const std::vector<TextModifierGroup*>& modifierGroups() const
    {
        return m_modifierGroups;
    }
    const SimpleArray<Paragraph>& shape() const { return m_shape; }
    const std::vector<Unichar>& unichars() const
    {
        return m_styledText.unichars();
    }
#endif

protected:
    void alignValueChanged() override;
    void sizingValueChanged() override;
    void overflowValueChanged() override;
    void widthChanged() override;
    void heightChanged() override;
    void paragraphSpacingChanged() override;
    bool makeStyled(StyledText& styledText, bool withModifiers = true) const;
    void originValueChanged() override;

private:
#ifdef WITH_RIVE_TEXT
    void updateOriginWorldTransform();
    std::vector<TextValueRun*> m_runs;
    std::vector<TextValueRun*> m_allRuns;
    std::vector<TextStylePaint*> m_renderStyles;
    SimpleArray<Paragraph> m_shape;
    SimpleArray<Paragraph> m_modifierShape;
    SimpleArray<SimpleArray<GlyphLine>> m_lines;
    SimpleArray<SimpleArray<GlyphLine>> m_modifierLines;
    // Runs ordered by paragraph line.
    std::vector<OrderedLine> m_orderedLines;
    GlyphRun m_ellipsisRun;
    RawPath m_clipRect;
    ShapePaintPath m_clipPath;
    AABB m_bounds;
    std::vector<TextModifierGroup*> m_modifierGroups;

    StyledText m_styledText;
    StyledText m_modifierStyledText;
    GlyphLookup m_glyphLookup;
    std::vector<TextStylePaint*> m_textStylePaints;

    void clearRenderStyles();
    TextBoundsInfo computeBoundsInfo();
    LineIter shouldDrawLine(float y, float totalHeight, const GlyphLine& line);
    void buildTextStylePaints();
    std::vector<TextValueRunListener*> m_valueRunListeners;

#endif
    float m_layoutWidth = NAN;
    float m_layoutHeight = NAN;
    uint8_t m_layoutWidthScaleType = std::numeric_limits<uint8_t>::max();
    uint8_t m_layoutHeightScaleType = std::numeric_limits<uint8_t>::max();
    LayoutDirection m_layoutDirection = LayoutDirection::inherit;
    Vec2D measure(Vec2D maxSize);
};
} // namespace rive

#endif
