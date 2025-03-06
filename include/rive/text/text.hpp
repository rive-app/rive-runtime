#ifndef _RIVE_TEXT_CORE_HPP_
#define _RIVE_TEXT_CORE_HPP_
#include "rive/generated/text/text_base.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/rect.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text_engine.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/simple_array.hpp"
#include <unordered_map>
#include <vector>
#include "rive/text/glyph_lookup.hpp"
namespace rive
{

enum class TextSizing : uint8_t
{
    autoWidth,
    autoHeight,
    fixed
};

enum class TextOverflow : uint8_t
{
    visible,
    hidden,
    clipped,
    ellipsis,
    fit,
};

enum class TextOrigin : uint8_t
{
    top,
    baseline
};

class OrderedLine;
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

// STL-style iterator for individual glyphs in a line, simplfies call sites from
// needing to iterate both runs and glyphs within those runs per line. A single
// iterator allows iterating all the glyphs in the line and provides the correct
// run they belong to (this also takes into account bidi which can put the runs
// in different order from how they were provided by the line breaker).
//
//   for (auto [run, glyphIndex] : orderedLine) { ... }
//
class GlyphItr
{
public:
    GlyphItr() = default;
    GlyphItr(const OrderedLine* line,
             const rive::GlyphRun* const* run,
             uint32_t glyphIndex) :
        m_line(line), m_run(run), m_glyphIndex(glyphIndex)
    {}

    void tryAdvanceRun();

    bool operator!=(const GlyphItr& that) const
    {
        return m_run != that.m_run || m_glyphIndex != that.m_glyphIndex;
    }
    bool operator==(const GlyphItr& that) const
    {
        return m_run == that.m_run && m_glyphIndex == that.m_glyphIndex;
    }

    GlyphItr& operator++();

    std::tuple<const GlyphRun*, uint32_t> operator*() const
    {
        return {*m_run, m_glyphIndex};
    }

private:
    const OrderedLine* m_line;
    const rive::GlyphRun* const* m_run;
    uint32_t m_glyphIndex;
};

// Represents a line of text with runs ordered visually. Also tracks logical
// start/end which will defer when using bidi.
class OrderedLine
{
public:
    OrderedLine(const Paragraph& paragraph,
                const GlyphLine& line,
                float lineWidth, // for ellipsis
                bool wantEllipsis,
                bool isEllipsisLineLast,
                GlyphRun* ellipsisRun);

    bool buildEllipsisRuns(std::vector<const GlyphRun*>& logicalRuns,
                           const Paragraph& paragraph,
                           const GlyphLine& line,
                           float lineWidth,
                           bool isEllipsisLineLast,
                           GlyphRun* ellipsisRun);
    const GlyphRun* startLogical() const { return m_startLogical; }
    const GlyphRun* endLogical() const { return m_endLogical; }
    const std::vector<const GlyphRun*>& runs() const { return m_runs; }

    GlyphItr begin() const
    {
        auto runItr = m_runs.data();
        auto itr = GlyphItr(this, runItr, startGlyphIndex(*runItr));
        itr.tryAdvanceRun();
        return itr;
    }

    GlyphItr end() const
    {
        auto runItr =
            m_runs.data() + (m_runs.size() == 0 ? 0 : m_runs.size() - 1);
        return GlyphItr(this, runItr, endGlyphIndex(*runItr));
    }

private:
    const GlyphRun* m_startLogical = nullptr;
    const GlyphRun* m_endLogical = nullptr;
    uint32_t m_startGlyphIndex;
    uint32_t m_endGlyphIndex;
    std::vector<const GlyphRun*> m_runs;

public:
    const GlyphRun* lastRun() const { return m_runs.back(); }
    uint32_t startGlyphIndex(const GlyphRun* run) const
    {
        TextDirection dir =
            run->level & 1 ? TextDirection::rtl : TextDirection::ltr;
        switch (dir)
        {
            case TextDirection::ltr:
                return m_startLogical == run ? m_startGlyphIndex : 0;
            case TextDirection::rtl:
                return (m_endLogical == run ? m_endGlyphIndex
                                            : (uint32_t)run->glyphs.size()) -
                       1;
        }
        RIVE_UNREACHABLE();
    }
    uint32_t endGlyphIndex(const GlyphRun* run) const
    {
        TextDirection dir =
            run->level & 1 ? TextDirection::rtl : TextDirection::ltr;
        switch (dir)
        {
            case TextDirection::ltr:
                return m_endLogical == run ? m_endGlyphIndex
                                           : (uint32_t)run->glyphs.size();
            case TextDirection::rtl:
                return (m_startLogical == run ? m_startGlyphIndex : 0) - 1;
        }
        RIVE_UNREACHABLE();
    }
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

class TextStyle;
class Text : public TextBase
{
public:
    void draw(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    void addRun(TextValueRun* run);
    void addModifierGroup(TextModifierGroup* group);
    void markShapeDirty(bool sendToLayout = true);
    void modifierShapeDirty();
    void markPaintDirty();
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
    const TextStyle* styleFromShaperId(uint16_t id) const;
    bool modifierRangesNeedShape() const;
    AABB localBounds() const override;
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
#ifdef WITH_RIVE_TEXT
    const std::vector<TextValueRun*>& runs() const { return m_runs; }
    static SimpleArray<SimpleArray<GlyphLine>> BreakLines(
        const SimpleArray<Paragraph>& paragraphs,
        float width,
        TextAlign align,
        TextWrap wrap);
#endif

#ifdef WITH_RIVE_LAYOUT
    void markLayoutNodeDirty();
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
    std::vector<TextStyle*> m_renderStyles;
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

    std::unordered_map<uint16_t, std::vector<Rect>> m_textValueRunToRects;
    void clearRenderStyles();
    TextBoundsInfo computeBoundsInfo();
    LineIter shouldDrawLine(float y, float totalHeight, const GlyphLine& line);

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
