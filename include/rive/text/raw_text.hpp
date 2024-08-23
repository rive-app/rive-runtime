#ifndef _RIVE_RENDER_TEXT_HPP_
#define _RIVE_RENDER_TEXT_HPP_

#ifdef WITH_RIVE_TEXT

#include "rive/text/text.hpp"

namespace rive
{
class Factory;

class RawText
{
public:
    RawText(Factory* factory);

    /// Returns true if the text object contains no text.
    bool empty() const;

    /// Appends a run to the text object.
    void append(const std::string& text,
                rcp<RenderPaint> paint,
                rcp<Font> font,
                float size = 16.0f,
                float lineHeight = -1.0f,
                float letterSpacing = 0.0f);

    /// Resets the text object to empty state (no text).
    void clear();

    /// Draw the text using renderer. Second argument is optional to override
    /// all paints provided with run styles
    void render(Renderer* renderer, rcp<RenderPaint> paint = nullptr);

    TextSizing sizing() const;
    TextOverflow overflow() const;
    TextAlign align() const;
    float maxWidth() const;
    float maxHeight() const;
    float paragraphSpacing() const;

    void sizing(TextSizing value);

    /// How text that overflows when TextSizing::fixed is used.
    void overflow(TextOverflow value);

    /// How text aligns within the bounds.
    void align(TextAlign value);

    /// The width at which the text will wrap when using any sizing but TextSizing::auto.
    void maxWidth(float value);

    /// The height at which the text will overflow when using TextSizing::fixed.
    void maxHeight(float value);

    /// The vertical space between paragraphs delineated by a return character.
    void paragraphSpacing(float value);

    /// Returns the bounds of the text object (helpful for aligning multiple
    /// text objects/procredurally drawn shapes).
    AABB bounds();

private:
    void update();
    struct RenderStyle
    {
        rcp<RenderPaint> paint;
        rcp<RenderPath> path;
        bool isEmpty;
    };
    SimpleArray<Paragraph> m_shape;
    SimpleArray<SimpleArray<GlyphLine>> m_lines;

    StyledText m_styled;
    Factory* m_factory;
    std::vector<RenderStyle> m_styles;
    std::vector<RenderStyle*> m_renderStyles;
    bool m_dirty = false;
    float m_paragraphSpacing = 0.0f;

    TextOrigin m_origin = TextOrigin::top;
    TextSizing m_sizing = TextSizing::autoWidth;
    TextOverflow m_overflow = TextOverflow::visible;
    TextAlign m_align = TextAlign::left;
    float m_maxWidth = 0.0f;
    float m_maxHeight = 0.0f;
    std::vector<OrderedLine> m_orderedLines;
    GlyphRun m_ellipsisRun;
    AABB m_bounds;
    rcp<RenderPath> m_clipRenderPath;
};
} // namespace rive

#endif // WITH_RIVE_TEXT

#endif
