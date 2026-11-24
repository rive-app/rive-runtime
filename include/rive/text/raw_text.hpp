#ifndef _RIVE_RENDER_TEXT_HPP_
#define _RIVE_RENDER_TEXT_HPP_

#ifdef WITH_RIVE_TEXT

#include "rive/text/text.hpp"
#include "rive/text/glyph_lookup.hpp"
#include "rive/text/text_layout_view.hpp"
#include "rive/text/text_selection_path.hpp"
#include <memory>
#include <vector>

namespace rive
{
class Factory;

class RawText
{
public:
  RawText(Factory* factory);

  /// Returns true if the text object contains no text.
  bool empty() const;

  /// Lines a run can carry, each drawn with its own paint (none when null):
  /// a highlight behind the line box, and an underline and a strikethrough
  /// placed and sized by the run's font. A mark joins neighbouring runs'
  /// marks of the same kind and paint into one bar per line, whatever their
  /// glyph paints.
  struct RunMarks
  {
    rcp<RenderPaint> highlight;
    rcp<RenderPaint> underline;
    rcp<RenderPaint> strikethrough;

    bool any() const
    {
      return highlight != nullptr || underline != nullptr ||
             strikethrough != nullptr;
    }
  };

  /// Appends a run to the text object. The foregroundColor is used for
  /// color glyphs (emoji) that reference the text's foreground color.
  void append(
      const std::string& text,
      rcp<RenderPaint> paint,
      rcp<Font> font,
      float size = 16.0f,
      float lineHeight = -1.0f,
      float letterSpacing = 0.0f,
      ColorInt foregroundColor = 0xFF000000,
      const RunMarks& marks = RunMarks());

  /// Resets the text object to empty state (no text).
  void clear();

  /// Draw the text using renderer. Second argument is optional to override
  /// all paints provided with run styles, underlines and strikethroughs
  /// included; highlights keep their own. Highlights draw first, then
  /// underlines, the glyphs, and strikethroughs.
  void render(Renderer* renderer, rcp<RenderPaint> paint = nullptr);

  /// Draws only lines first to last (zero based and inclusive, clamped to
  /// the lines there are) where they sit in the full layout, so a paragraph
  /// can be split across pages. The last few ranges drawn keep their paths
  /// until the text changes.
  void renderLines(
      Renderer* renderer,
      uint32_t first,
      uint32_t last,
      rcp<RenderPaint> paint = nullptr);

  // Resolved against the first paragraph's direction after shaping, so right
  // to left text starts on the right without the caller knowing the language.
  enum class LogicalAlign : uint8_t
  {
    none,
    start,
    end
  };

  TextSizing sizing() const;
  TextOverflow overflow() const;
  TextAlign align() const;
  LogicalAlign logicalAlign() const;
  TextWrap wrap() const;
  TextWordBreak wordBreak() const;
  TextOrigin origin() const;
  // -1 detects each paragraph's direction, 0 forces left to right, 1 right
  // to left.
  int directionFlag() const;
  float maxWidth() const;
  float maxHeight() const;
  float paragraphSpacing() const;

  void sizing(TextSizing value);

  /// How text that overflows when TextSizing::fixed is used.
  void overflow(TextOverflow value);

  /// How text aligns within the bounds.
  void align(TextAlign value);

  /// The width at which the text will wrap when using any sizing but
  /// TextSizing::auto.
  void maxWidth(float value);

  /// The height at which the text will overflow when using TextSizing::fixed.
  void maxHeight(float value);

  /// The vertical space between paragraphs delineated by a return character.
  void paragraphSpacing(float value);

  void logicalAlign(LogicalAlign value);
  void wrap(TextWrap value);
  void wordBreak(TextWordBreak value);
  void origin(TextOrigin value);
  void directionFlag(int value);

  // The ordinals the script bindings speak: align 0..2 is left, right and
  // center, 3 and 4 are start and end, and 5 is justify; direction 0
  // detects, 1 and 2 force. Out of range values leave the setting alone.
  int alignIndex() const;
  void alignIndex(int value);
  int directionIndex() const { return m_directionFlag + 1; }
  void directionIndex(int value);
  void sizingIndex(int value);
  void overflowIndex(int value);
  void wrapIndex(int value);
  void wordBreakIndex(int value);
  void originIndex(int value);

  /// Codepoints appended so far.
  size_t length() const { return m_styled.unichars().size(); }

  /// Bumps on every change that reshapes, so lines and glyphs handed out
  /// earlier can tell they no longer describe this layout.
  uint32_t revision() const { return m_revision; }

  /// Lines as drawn, glyphs in visual order. Shapes first when needed.
  const std::vector<OrderedLine>& orderedLines();
  uint32_t lineCount() { return (uint32_t)orderedLines().size(); }
  float lineTop(uint32_t line);
  TextDirection lineDirection(uint32_t line);
  uint32_t lineGlyphCount(uint32_t line);

  /// Visits a line's glyphs in visual order with the pen position each
  /// draws at, the shaper's offset included, until visit returns false.
  template <typename F> void forEachGlyph(uint32_t line, F&& visit)
  {
    const OrderedLine& orderedLine = orderedLines()[line];
    float x = orderedLine.startX();
    float y = orderedLine.y();
    for (auto [run, glyphIndex] : orderedLine)
    {
      const Vec2D& offset = run->offsets[glyphIndex];
      if (!visit(*run, glyphIndex, Vec2D(x + offset.x, y + offset.y)))
      {
        return;
      }
      x += orderedLine.advance(*run, glyphIndex);
    }
  }

  /// A glyph's advance as its line places it, a justified line's whitespace
  /// gap included.
  float glyphAdvance(uint32_t line, const GlyphRun& run, uint32_t glyphIndex)
  {
    return orderedLines()[line].advance(run, glyphIndex);
  }

  /// Borrowed layout for cursor math. Valid until the next change.
  TextLayoutView layoutView();

  RenderPaint* stylePaint(uint16_t styleId) const;
  ColorInt styleForegroundColor(uint16_t styleId) const;
  size_t styleCount() const { return m_styles.size(); }

  /// Places a run's glyph outline, 1 point units, at its pen position.
  static Mat2D glyphTransform(const GlyphRun& run, Vec2D position)
  {
    return Mat2D(run.size, 0.0f, 0.0f, run.size, position.x, position.y);
  }

  /// A glyph outline wound so a clockwise fill rule fills it.
  static RawPath glyphPath(const Font& font, GlyphID glyphId);

  /// Returns the bounds of the text object (helpful for aligning multiple
  /// text objects/procredurally drawn shapes).
  AABB bounds();

private:
  void update();
  // Shapes, breaks and orders the lines and builds the glyph paths; update
  // then joins the marks.
  void layoutLines();
  void drawColorGlyph(
      Renderer* renderer,
      const Font& font,
      GlyphID glyphId,
      const Mat2D& transform,
      ColorInt foregroundColor);
  void markDirty();
  // RunMarks' order.
  enum MarkKind
  {
    kHighlight = 0,
    kUnderline = 1,
    kStrikethrough = 2,
    kMarkKindCount = 3
  };
  // A mark kind with its paint, and the bars its glyphs' rects join into.
  // Every style whose runs have it adds to the same group, whatever their
  // glyph paints, so neighbouring runs join into one bar per line.
  struct MarkGroup
  {
    MarkKind kind = kHighlight;
    rcp<RenderPaint> paint;
    std::vector<AABB> rects;
    TextSelectionPath path;
  };
  static constexpr uint32_t kNoMarkGroup = 0xFFFFFFFF;
  struct RenderStyle
  {
    rcp<RenderPaint> paint;
    bool isEmpty;
    ShapePaintPath path;
    ColorInt foregroundColor = 0xFF000000;
    // Its runs' MarkGroup per MarkKind, kNoMarkGroup for none.
    uint32_t markGroups[kMarkKindCount] = {
        kNoMarkGroup,
        kNoMarkGroup,
        kNoMarkGroup};
  };
  // The MarkGroup drawing kind in paint, added if it's new; kNoMarkGroup
  // without a paint.
  uint32_t markGroup(MarkKind kind, const rcp<RenderPaint>& paint);
  SimpleArray<Paragraph> m_shape;
  SimpleArray<SimpleArray<GlyphLine>> m_lines;

  StyledText m_styled;
  Factory* m_factory;
  std::vector<RenderStyle> m_styles;
  std::vector<MarkGroup> m_markGroups;
  bool m_dirty = false;
  bool m_lookupDirty = false;
  uint32_t m_revision = 0;
  float m_paragraphSpacing = 0.0f;

  TextOrigin m_origin = TextOrigin::top;
  TextSizing m_sizing = TextSizing::autoWidth;
  TextOverflow m_overflow = TextOverflow::visible;
  TextAlign m_align = TextAlign::left;
  LogicalAlign m_logicalAlign = LogicalAlign::none;
  TextWrap m_wrap = TextWrap::wrap;
  TextWordBreak m_wordBreak = TextWordBreak::breakWord;
  int m_directionFlag = -1;
  GlyphLookup m_glyphLookup;
  float m_maxWidth = 0.0f;
  float m_maxHeight = 0.0f;
  std::vector<OrderedLine> m_orderedLines;
  // Per drawn line, filled beside m_orderedLines.
  std::vector<uint8_t> m_lineDirections;
  std::vector<uint32_t> m_lineGlyphCounts;
  GlyphRun m_ellipsisRun;
  AABB m_bounds;
  rcp<RenderPath> m_clipRenderPath;

  // Color glyph draw commands for interleaving with style paths.
  struct RawTextDrawCommand
  {
    enum Type
    {
      kStylePath,
      kColorGlyph
    };
    Type type;
    RenderStyle* style = nullptr;
    struct ColorGlyphInfo
    {
      rcp<Font> font;
      GlyphID glyphId;
      Mat2D transform;
      ColorInt foregroundColor;
    };
    ColorGlyphInfo colorGlyph;
  };
  std::vector<RawTextDrawCommand> m_drawCommands;

  // Adds a line's glyphs to the styles' paths and their marks to
  // markGroups, recording a command in draw order the first time a style
  // gets a glyph and for every color glyph. Returns the line's glyph count.
  uint32_t appendLineGlyphs(
      const OrderedLine& line,
      std::vector<RenderStyle>& styles,
      std::vector<MarkGroup>& markGroups,
      std::vector<RawTextDrawCommand>& commands);
  // Joins each group's mark rects into its bars.
  void updateMarks(std::vector<MarkGroup>& markGroups);
  void drawMarks(
      Renderer* renderer,
      std::vector<MarkGroup>& markGroups,
      MarkKind kind,
      RenderPaint* paint);
  void drawCommands(
      Renderer* renderer,
      const std::vector<RawTextDrawCommand>& commands,
      std::vector<MarkGroup>& markGroups,
      RenderPaint* paint);

  // The paths and commands of a range of lines drawn on its own.
  struct LineRange
  {
    uint32_t first;
    uint32_t last;
    std::vector<RenderStyle> styles;
    // m_markGroups' kinds and paints, with this range's bars.
    std::vector<MarkGroup> markGroups;
    std::vector<RawTextDrawCommand> commands;
  };
  LineRange& lineRange(uint32_t first, uint32_t last);
  // Enough for a paragraph split across a few pages, drawn every frame.
  static constexpr size_t kMaxLineRanges = 4;
  // Most recently drawn last; emptied whenever the layout is rebuilt.
  std::vector<std::unique_ptr<LineRange>> m_lineRanges;
};
} // namespace rive

#endif // WITH_RIVE_TEXT

#endif
