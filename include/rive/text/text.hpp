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
    GlyphItr(const OrderedLine* line, const GlyphRun* run, size_t glyphIndex) :
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

    std::tuple<const GlyphRun*, size_t> operator*() const { return {m_run, m_glyphIndex}; }

private:
    const OrderedLine* m_line;
    const GlyphRun* m_run;
    size_t m_glyphIndex;
};

// Represents a line of text with runs ordered visually. Also tracks logical
// start/end which will defer when using bidi.
class OrderedLine
{
public:
    OrderedLine(const Paragraph& paragraph, const GlyphLine& line);
    const GlyphRun* startLogical() const { return m_startLogical; }
    const GlyphRun* endLogical() const { return m_endLogical; }
    const GlyphLine* glyphLine() const { return m_line; }
    const std::vector<const GlyphRun*>& runs() const { return m_runs; }

    GlyphItr begin() const
    {
        auto run = m_runs[0];
        return GlyphItr(this, run, startGlyphIndex(run));
    }

    GlyphItr end() const
    {
        auto run = lastRun();
        return GlyphItr(this, run, endGlyphIndex(run));
    }

private:
    const GlyphRun* m_startLogical;
    const GlyphRun* m_endLogical;
    const GlyphLine* m_line;
    std::vector<const GlyphRun*> m_runs;

public:
    const GlyphRun* lastRun() const { return m_runs.back(); }
    size_t startGlyphIndex(const GlyphRun* run) const
    {
        switch (run->dir)
        {
            case TextDirection::ltr:
                return m_startLogical == run ? m_line->startGlyphIndex : 0;
            case TextDirection::rtl:
                return (m_endLogical == run ? m_line->endGlyphIndex : run->glyphs.size()) - 1;
        }
        RIVE_UNREACHABLE();
    }
    size_t endGlyphIndex(const GlyphRun* run) const
    {
        switch (run->dir)
        {
            case TextDirection::ltr:
                return m_endLogical == run ? m_line->endGlyphIndex : run->glyphs.size();
            case TextDirection::rtl:
                return (m_startLogical == run ? m_line->startGlyphIndex : 0) - 1;
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
    void buildRenderStyles();

protected:
    void alignValueChanged() override;
    void sizingValueChanged() override;
    void overflowValueChanged() override;
    void widthChanged() override;
    void heightChanged() override;

private:
#ifdef WITH_RIVE_TEXT
    std::vector<TextValueRun*> m_runs;
    std::vector<TextStyle*> m_renderStyles;
    SimpleArray<Paragraph> m_shape;
    SimpleArray<SimpleArray<GlyphLine>> m_lines;
    // Runs ordered by paragraph line.
    std::vector<OrderedLine> m_orderedLines;
#endif
};
} // namespace rive

#endif