#ifndef _RIVE_RAW_TEXT_INPUT_HPP_
#define _RIVE_RAW_TEXT_INPUT_HPP_

#ifdef WITH_RIVE_TEXT
#include "rive/text/cursor.hpp"
#include "rive/text/text.hpp"
#include "rive/text/fully_shaped_text.hpp"
#include "rive/text/text_selection_path.hpp"

namespace rive
{
class Factory;

enum class CursorBoundary : uint8_t
{
    character,
    word,
    subWord,
    line
};

class RawTextInput
{
public:
#ifdef TESTING
    uint32_t measureCount = 0;
#endif
    RawTextInput();

    void draw(Factory* factory,
              Renderer* renderer,
              const Mat2D& worldTransform,
              RenderPaint* textPaint,
              RenderPaint* selectionPaint,
              RenderPaint* cursorPaint);

    float fontSize() const { return m_textRun.size; }
    void fontSize(float value);

    float maxWidth() const { return m_maxWidth; }
    void maxWidth(float value);

    float maxHeight() const { return m_maxHeight; }
    void maxHeight(float value);

    TextSizing sizing() const { return m_sizing; }
    void sizing(TextSizing value);

    TextOverflow overflow() const { return m_overflow; }
    void overflow(TextOverflow value);

    rcp<Font> font() const { return m_textRun.font; }
    void font(rcp<Font> value);

    float paragraphSpacing() const { return m_paragraphSpacing; }
    void paragraphSpacing(float value);

    float selectionCornerRadius() const { return m_selectionCornerRadius; }
    void selectionCornerRadius(float value);

    bool separateSelectionText() const;
    void separateSelectionText(bool value);

    enum class Flags : uint8_t
    {
        none = 0,
        shapeDirty = 1 << 0,
        selectionDirty = 1 << 1,
        separateSelectionText = 1 << 2,
        measureDirty = 1 << 3
    };

    Flags update(Factory* factory);

    ShapePaintPath* textPath() { return &m_textPath; }
    ShapePaintPath* selectedTextPath() { return &m_selectedTextPath; }
    ShapePaintPath* cursorPath() { return &m_cursorPath; }
    ShapePaintPath* selectionPath() { return &m_selectionPath; }
    rcp<RenderPath> clipRenderPath() { return m_clipRenderPath; }

    /// Returns the bounds of the text object (helpful for aligning multiple
    /// text objects/procredurally drawn shapes).
    AABB bounds();
    CursorVisualPosition cursorVisualPosition(CursorPosition position)
    {
        return position.visualPosition(m_shape);
    }

    CursorVisualPosition cursorVisualPosition() const
    {
        return m_cursorVisualPosition;
    }

    void insert(const std::string& text);
    void insert(Unichar codePoint);
    void erase();
    void backspace(int32_t direction);

    void cursorLeft(CursorBoundary boundary = CursorBoundary::character,
                    bool select = false);
    void cursorRight(CursorBoundary boundary = CursorBoundary::character,
                     bool select = false);
    void cursorUp(bool select = false);
    void cursorDown(bool select = false);

    void moveCursorTo(Vec2D translation, bool select = false);
    const FullyShapedText& shape() const { return m_shape; }
    const Cursor cursor() const { return m_cursor; }
    void cursor(Cursor value);

    // Selects the word at the cursor.
    void selectWord();

    std::string text() const;
    void text(std::string value);
    // Length of the input text.
    size_t length() const;

    // Returns true if the input is empty.
    bool empty() const;

    void undo();
    void redo();

    AABB measure(float maxWidth, float maxHeight);

    enum class Delineator : uint8_t
    {
        unknown = 0,
        lowercase = 1 << 0,
        uppercase = 1 << 1,
        symbol = 1 << 2,
        underscore = 1 << 3,
        whitespace = 1 << 4,
        word = lowercase | uppercase | underscore,
        any =
            lowercase | uppercase | symbol | underscore | whitespace | lowercase

    };

private:
    void captureJournalEntry(Cursor cursor);
    void cursorHorizontal(int32_t offset, CursorBoundary boundary, bool select);
    static Delineator classify(Unichar codepoint);
    Delineator classify(CursorPosition position) const;

    // Hunt for a delineator bit maks from position in direction. Returns what
    // was actually found. The position will be updated to the location where
    // the Delineator was found.
    Delineator find(uint8_t delineatorMask,
                    CursorPosition& position,
                    int32_t direction) const;

    CursorPosition findPosition(uint8_t delineatorMask,
                                const CursorPosition& position,
                                int32_t direction) const;

    const OrderedLine* orderedLine(CursorPosition position) const;

    void buildTextPaths(Factory* factory);
    void computeVisualPositionFromCursor();
    void setTextPrivate(std::string value);
    bool flagged(Flags mask) const;
    bool unflag(Flags mask);
    void flag(Flags mask);

    Cursor m_cursor;
    TextRun m_textRun;
    ShapePaintPath m_textPath;
    ShapePaintPath m_selectedTextPath;
    ShapePaintPath m_cursorPath;
    TextSelectionPath m_selectionPath;
    std::vector<Unichar> m_text;

    FullyShapedText m_shape;
    std::unique_ptr<FullyShapedText> m_measuringShape;
    float m_lastMeasureMaxWidth = 0.0f;
    float m_lastMeasureMaxHeight = 0.0f;

    Flags m_flags = Flags::none;

    float m_paragraphSpacing = 0.0f;
    TextOrigin m_origin = TextOrigin::top;
    TextSizing m_sizing = TextSizing::autoWidth;
    TextOverflow m_overflow = TextOverflow::visible;
    TextAlign m_align = TextAlign::left;
    TextWrap m_wrap = TextWrap::wrap;
    float m_maxWidth = 0.0f;
    float m_maxHeight = 0.0f;
    rcp<RenderPath> m_clipRenderPath;
    float m_idealCursorX = -1.0f;

    CursorVisualPosition m_cursorVisualPosition;
    std::vector<AABB> m_selectionRects;

    float m_selectionCornerRadius = 5.0f;

    struct JournalEntry
    {
        Cursor cursorFrom;
        Cursor cursorTo;
        std::string text;
    };
    std::vector<JournalEntry> m_journal;
    uint32_t m_journalIndex = 0;
};

// Helper for performing bitwise and with Delineator.
inline uint8_t operator&(const RawTextInput::Delineator& delineator,
                         const uint8_t mask)
{
    return (uint8_t)delineator & mask;
}

inline uint8_t operator~(const RawTextInput::Delineator& delineator)
{
    return ~(uint8_t)delineator;
}

inline RawTextInput::Delineator operator|(const RawTextInput::Delineator& a,
                                          const RawTextInput::Delineator& b)
{
    return (RawTextInput::Delineator)((uint8_t)a | (uint8_t)b);
}

inline RawTextInput::Delineator operator&(const RawTextInput::Delineator& a,
                                          const RawTextInput::Delineator& b)
{
    return (RawTextInput::Delineator)((uint8_t)a & (uint8_t)b);
}
RIVE_MAKE_ENUM_BITSET(RawTextInput::Flags);
} // namespace rive

#endif
#endif