#ifndef _RIVE_TEXT_CORE_HPP_
#define _RIVE_TEXT_CORE_HPP_
#include "rive/generated/text/text_base.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text_engine.hpp"
#include "rive/simple_array.hpp"

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
    ellipsis
};

class OrderedLine;

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
    GlyphItr(const OrderedLine* line, const rive::GlyphRun* const* run, uint32_t glyphIndex) :
        m_line(line), m_run(run), m_glyphIndex(glyphIndex)
    {}

    bool operator!=(const GlyphItr& that) const
    {
        return m_run != that.m_run || m_glyphIndex != that.m_glyphIndex;
    }
    bool operator==(const GlyphItr& that) const
    {
        return m_run == that.m_run && m_glyphIndex == that.m_glyphIndex;
    }

    GlyphItr& operator++();

    std::tuple<const GlyphRun*, uint32_t> operator*() const { return {*m_run, m_glyphIndex}; }

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
        return GlyphItr(this, runItr, startGlyphIndex(*runItr));
    }

    GlyphItr end() const
    {
        auto runItr = m_runs.data() + (m_runs.size() == 0 ? 0 : m_runs.size() - 1);
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
        switch (run->dir)
        {
            case TextDirection::ltr:
                return m_startLogical == run ? m_startGlyphIndex : 0;
            case TextDirection::rtl:
                return (m_endLogical == run ? m_endGlyphIndex : (uint32_t)run->glyphs.size()) - 1;
        }
        RIVE_UNREACHABLE();
    }
    uint32_t endGlyphIndex(const GlyphRun* run) const
    {
        switch (run->dir)
        {
            case TextDirection::ltr:
                return m_endLogical == run ? m_endGlyphIndex : (uint32_t)run->glyphs.size();
            case TextDirection::rtl:
                return (m_startLogical == run ? m_startGlyphIndex : 0) - 1;
        }
        RIVE_UNREACHABLE();
    }
};

class TextStyle;
class Text : public TextBase
{
public:
    void draw(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    void addRun(TextValueRun* run);
    void markShapeDirty();
    void update(ComponentDirt value) override;

    TextSizing sizing() { return (TextSizing)sizingValue(); }
    TextOverflow overflow() { return (TextOverflow)overflowValue(); }
    void overflow(TextOverflow value) { return overflowValue((uint32_t)value); }
    void buildRenderStyles();

#ifdef TESTING
    std::vector<OrderedLine>& orderedLines() { return m_orderedLines; }
#endif

protected:
    void alignValueChanged() override;
    void sizingValueChanged() override;
    void overflowValueChanged() override;
    void widthChanged() override;
    void heightChanged() override;

private:
#ifdef WITH_RIVE_TEXT
    void updateOriginWorldTransform();
    std::vector<TextValueRun*> m_runs;
    std::vector<TextStyle*> m_renderStyles;
    SimpleArray<Paragraph> m_shape;
    SimpleArray<SimpleArray<GlyphLine>> m_lines;
    // Runs ordered by paragraph line.
    std::vector<OrderedLine> m_orderedLines;
    GlyphRun m_ellipsisRun;
    std::unique_ptr<RenderPath> m_clipRenderPath;
    Mat2D m_originWorldTransform;
    float m_actualWidth = 0.0f;
    float m_actualHeight = 0.0f;
#endif
};
} // namespace rive

#endif