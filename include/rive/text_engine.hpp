/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_TEXT_ENGINE_HPP_
#define _RIVE_TEXT_ENGINE_HPP_

#include "rive/math/raw_path.hpp"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"
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
    ellipsis,
    fit,
};

enum class TextOrigin : uint8_t
{
    top,
    baseline
};

// Representation of a single unicode codepoint.
using Unichar = uint32_t;
// Id for a glyph within a font.
using GlyphID = uint16_t;

struct TextRun;
struct GlyphRun;

bool isWhiteSpace(Unichar c);

// Direction a paragraph or run flows in.
enum class TextDirection : uint8_t
{
    ltr = 0,
    rtl = 1
};

// The alignment of each word wrapped line in a paragraph.
enum class TextAlign : uint8_t
{
    left = 0,
    right = 1,
    center = 2
};

// The wrap mode.
enum class TextWrap : uint8_t
{
    wrap = 0,
    noWrap = 1
};

// The alignment of each word wrapped line in a paragraph.
enum class VerticalTextAlign : uint8_t
{
    top = 0,
    bottom = 1,
    middle = 2
};

// A horizontal line of text within a paragraph, after line-breaking.
struct GlyphLine
{
    uint32_t startRunIndex;
    uint32_t startGlyphIndex;
    uint32_t endRunIndex;
    uint32_t endGlyphIndex;
    float startX;
    float top = 0, baseline = 0, bottom = 0;

    bool operator==(const GlyphLine& o) const
    {
        return startRunIndex == o.startRunIndex &&
               startGlyphIndex == o.startGlyphIndex &&
               endRunIndex == o.endRunIndex && endGlyphIndex == o.endGlyphIndex;
    }

    GlyphLine() :
        startRunIndex(0),
        startGlyphIndex(0),
        endRunIndex(0),
        endGlyphIndex(0),
        startX(0.0f)
    {}
    GlyphLine(uint32_t run, uint32_t index) :
        startRunIndex(run),
        startGlyphIndex(index),
        endRunIndex(run),
        endGlyphIndex(index),
        startX(0.0f)
    {}

    bool empty() const
    {
        return startRunIndex == endRunIndex && startGlyphIndex == endGlyphIndex;
    }

    static SimpleArray<GlyphLine> BreakLines(Span<const GlyphRun> runs,
                                             float width);

    // Compute values for top/baseline/bottom per line
    static void ComputeLineSpacing(bool isFirstLine,
                                   Span<GlyphLine>,
                                   Span<const GlyphRun>,
                                   float width,
                                   TextAlign align);

    static float ComputeMaxWidth(Span<GlyphLine> lines,
                                 Span<const GlyphRun> runs);
};

// A paragraph represents of set of runs that flow in a specific direction. The
// runs are always provided in LTR and must be drawn in reverse when the
// baseDirection is RTL. These are built by the system during shaping where the
// user provided string and text styling is converted to shaped paragraphs.
struct Paragraph
{
    SimpleArray<GlyphRun> runs;
    uint8_t level;
    TextDirection baseDirection() const
    {
        return level & 1 ? TextDirection::rtl : TextDirection::ltr;
    }
};

// An abstraction for interfacing with an individual font.
class Font : public RefCnt<Font>
{
public:
    virtual ~Font() {}

    struct LineMetrics
    {
        float ascent, descent;
    };

    const LineMetrics& lineMetrics() const { return m_lineMetrics; }

    float ascent(float size) const { return m_lineMetrics.ascent * size; }

    float descent(float size) const { return m_lineMetrics.descent * size; }

    // Variable axis available for the font.
    struct Axis
    {
        uint32_t tag;
        float min;
        float def; // default value
        float max;
    };

    // Variable axis setting.
    struct Coord
    {
        uint32_t axis;
        float value;
    };

    // Returns the count of variable axes available for this font.
    virtual uint16_t getAxisCount() const = 0;

    // Returns the definition of the Axis at the provided index.
    virtual Axis getAxis(uint16_t index) const = 0;

    // Value for the axis, if a Coord has been provided the value from the Coord
    // will be used. Otherwise the default value for the axis will be returned.
    virtual float getAxisValue(uint32_t axisTag) const = 0;

    // Returns the current font value as a numeric value [1, 1000]
    virtual uint16_t getWeight() const = 0;

    // Whether this font is italic or not.
    virtual bool isItalic() const = 0;

    // Font feature.
    struct Feature
    {
        uint32_t tag;
        uint32_t value;
    };

    // Returns the features available for this font.
    virtual SimpleArray<uint32_t> features() const = 0;

    virtual bool hasGlyph(const rive::Unichar) const = 0;

    // Value for the feature, if no value has been provided a (uint32_t)-1 is
    // returned to signal that the text engine will pick the best feature value
    // for the content.
    virtual uint32_t getFeatureValue(uint32_t featureTag) const = 0;

    rcp<Font> makeAtCoords(Span<const Coord> coords) const
    {
        return withOptions(coords, Span<const Feature>(nullptr, 0));
    }

    rcp<Font> makeAtCoord(Coord c)
    {
        return this->makeAtCoords(Span<const Coord>(&c, 1));
    }

    virtual rcp<Font> withOptions(Span<const Coord> variableAxes,
                                  Span<const Feature> features) const = 0;

    // Returns a 1-point path for this glyph. It will be positioned
    // relative to (0,0) with the typographic baseline at y = 0.
    //
    virtual RawPath getPath(GlyphID) const = 0;

    SimpleArray<Paragraph> shapeText(Span<const Unichar> text,
                                     Span<const TextRun> runs,
                                     int textDirectionFlag = -1) const;

    // If the platform can supply fallback font(s), set this function pointer.
    // It will be called with a span of unichars, and the platform attempts to
    // return a font that can draw (at least some of) them. If no font is
    // available just return nullptr.

    using FallbackProc = rive::rcp<rive::Font> (*)(const rive::Unichar missing,
                                                   const uint32_t fallbackIndex,
                                                   const rive::Font*);
    static FallbackProc gFallbackProc;
    static bool gFallbackProcEnabled;
    static constexpr unsigned kRegularWeight = 400;

protected:
    Font(const LineMetrics& lm) : m_lineMetrics(lm) {}

    virtual SimpleArray<Paragraph> onShapeText(Span<const Unichar> text,
                                               Span<const TextRun> runs,
                                               int textDirectionFlag) const = 0;

private:
    /// The font specified line metrics (automatic line metrics).
    const LineMetrics m_lineMetrics;
};

// A user defined styling guide for a set of unicode codepoints within a larger
// text string.
struct TextRun
{
    rcp<Font> font;
    float size;
    float lineHeight;
    float letterSpacing;
    uint32_t unicharCount;
    uint32_t script;
    uint16_t styleId;
    uint8_t level;
};

// The corresponding system generated run for the user provided TextRuns.
// GlyphRuns may not match TextRuns if the system needs to split the run (for
// fallback fonts) or if codepoints get ligated/shaped to a single glyph.
struct GlyphRun
{
    GlyphRun(size_t glyphCount = 0) :
        glyphs(glyphCount),
        textIndices(glyphCount),
        advances(glyphCount),
        xpos(glyphCount + 1),
        offsets(glyphCount)
    {}

    GlyphRun(SimpleArray<GlyphID> glyphIds,
             SimpleArray<uint32_t> offsets,
             SimpleArray<float> ws,
             SimpleArray<float> xs,
             SimpleArray<rive::Vec2D> offs) :
        glyphs(glyphIds),
        textIndices(offsets),
        advances(ws),
        xpos(xs),
        offsets(offs)
    {}

    rcp<Font> font;
    float size;
    float lineHeight;
    float letterSpacing;

    // List of glyphs, represented by font specific glyph ids. Length is equal
    // to number of glyphs in the run.
    SimpleArray<GlyphID> glyphs;

    // Index in the unicode text array representing the text displayed in this
    // run. Because each glyph can be composed of multiple unicode values, this
    // index points to the first index in the unicode text. Length is equal to
    // number of glyphs in the run.
    SimpleArray<uint32_t> textIndices;

    // X position of each glyph in visual order (xpos is in logical/memory
    // order).
    SimpleArray<float> advances;

    // X position of each glyph, with an extra value at the end for the right
    // most extent of the last glyph.
    SimpleArray<float> xpos;

    // X and Y offset each glyphs draws at relative to its baseline and advance
    // position.
    SimpleArray<rive::Vec2D> offsets;

    // List of possible indices to line break at. Has a stride of 2 uint32_ts
    // where each pair marks the start and end of a word, with the exception of
    // a return character (forced linebreak) which is represented as a 0 length
    // word (where start/end index is the same).
    SimpleArray<uint32_t> breaks;

    // The unique identifier for the styling (fill/stroke colors, anything not
    // determined by the font or font size) applied to this run.
    uint16_t styleId;

    // Bidi level (even is LTR, odd is RTL)
    uint8_t level;

    TextDirection dir() const
    {
        return level & 1 ? TextDirection::rtl : TextDirection::ltr;
    }
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

    const rive::GlyphRun* run() const { return *m_run; }
    uint32_t glyphIndex() const { return m_glyphIndex; }

private:
    const OrderedLine* m_line;
    const rive::GlyphRun* const* m_run;
    uint32_t m_glyphIndex;
};

class GlyphLookup;
// Represents a line of text with runs ordered visually. Also tracks logical
// start/end which will differ when using bidi.
class OrderedLine
{
public:
    OrderedLine(const Paragraph& paragraph,
                const GlyphLine& line,
                float lineWidth, // for ellipsis
                bool wantEllipsis,
                bool isEllipsisLineLast,
                GlyphRun* ellipsisRun,
                float y);

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

    const GlyphLine& glyphLine() const { return *m_glyphLine; }
    float y() const { return m_y; }
    float bottom() const;

    uint32_t firstCodePointIndex(const GlyphLookup& glyphLookup) const;
    uint32_t lastCodePointIndex(const GlyphLookup& glyphLookup) const;
    bool containsCodePointIndex(const GlyphLookup& glyphLookup,
                                uint32_t codePointIndex) const;

private:
    const GlyphRun* m_startLogical = nullptr;
    const GlyphRun* m_endLogical = nullptr;
    uint32_t m_startGlyphIndex;
    uint32_t m_endGlyphIndex;
    std::vector<const GlyphRun*> m_runs;
    const GlyphLine* m_glyphLine;
    float m_y;

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

} // namespace rive
#endif
