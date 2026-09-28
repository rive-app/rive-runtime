#ifndef _RIVE_RENDER_TEXT_HPP_
#define _RIVE_RENDER_TEXT_HPP_

#ifdef WITH_RIVE_TEXT

#include "rive/text/text.hpp"
#include "rive/text/glyph_lookup.hpp"
#include "rive/text/text_layout_view.hpp"

namespace rive
{
class Factory;

class RawText
{
public:
    RawText(Factory* factory);

    /// Returns true if the text object contains no text.
    bool empty() const;

    /// Appends a run to the text object. The foregroundColor is used for
    /// color glyphs (emoji) that reference the text's foreground color.
    void append(const std::string& text,
                rcp<RenderPaint> paint,
                rcp<Font> font,
                float size = 16.0f,
                float lineHeight = -1.0f,
                float letterSpacing = 0.0f,
                ColorInt foregroundColor = 0xFF000000);

    /// Resets the text object to empty state (no text).
    void clear();

    /// Draw the text using renderer. Second argument is optional to override
    /// all paints provided with run styles
    void render(Renderer* renderer, rcp<RenderPaint> paint = nullptr);

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

    // The ordinals the script bindings speak: align 0..2 is TextAlign and 3
    // and 4 are start and end; direction 0 detects, 1 and 2 force. Out of
    // range values leave the setting alone.
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
        float x = orderedLine.glyphLine().startX;
        float y = orderedLine.y();
        for (auto [run, glyphIndex] : orderedLine)
        {
            const Vec2D& offset = run->offsets[glyphIndex];
            if (!visit(*run, glyphIndex, Vec2D(x + offset.x, y + offset.y)))
            {
                return;
            }
            x += run->advances[glyphIndex];
        }
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
    void drawColorGlyph(Renderer* renderer,
                        const Font& font,
                        GlyphID glyphId,
                        const Mat2D& transform,
                        ColorInt foregroundColor);
    void markDirty();
    struct RenderStyle
    {
        rcp<RenderPaint> paint;
        bool isEmpty;
        ShapePaintPath path;
        ColorInt foregroundColor = 0xFF000000;
    };
    SimpleArray<Paragraph> m_shape;
    SimpleArray<SimpleArray<GlyphLine>> m_lines;

    StyledText m_styled;
    Factory* m_factory;
    std::vector<RenderStyle> m_styles;
    std::vector<RenderStyle*> m_renderStyles;
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
};
} // namespace rive

#endif // WITH_RIVE_TEXT

#endif
