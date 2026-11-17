#ifndef _RIVE_RAW_TEXT_INPUT_HPP_
#define _RIVE_RAW_TEXT_INPUT_HPP_

#ifdef WITH_RIVE_TEXT
#include "rive/text/cursor.hpp"
#include "rive/text/text.hpp"
#include "rive/text/fully_shaped_text.hpp"
#include "rive/text/text_selection_path.hpp"
#include <limits>

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
  // Shapes run to check maxLines.
  uint32_t limitShapeCount = 0;
  uint32_t fullShapeCount = 0;
  uint32_t reshapeCount = 0;
  // Code points the undo history holds.
  size_t journalCodePoints() const;
  // Lines whose glyph outlines were built.
  uint32_t lineBuildCount = 0;
#endif
  RawTextInput();

  void draw(
      Factory* factory,
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

  TextAlign align() const { return m_align; }
  void align(TextAlign value);

  /// The width lines are aligned within when it's wider than the text
  /// itself. Text wider than this aligns within its own width, which
  /// effectively left aligns it (and leaves horizontal scrolling intact).
  float alignWidth() const { return m_alignWidth; }
  void alignWidth(float value);

  VerticalTextAlign verticalAlign() const { return m_verticalAlign; }
  void verticalAlign(VerticalTextAlign value);

  /// The height the text block is aligned within, the vertical counterpart
  /// to alignWidth: text taller than this stays at the top, leaving
  /// vertical scrolling intact.
  float alignHeight() const { return m_alignHeight; }
  void alignHeight(float value);

  rcp<Font> font() const { return m_textRun.font; }
  void font(rcp<Font> value);

  /// The interned language the text shapes with (see TextLanguages), none
  /// to inherit the system's.
  TextLanguageId language() const { return m_textRun.language; }
  void language(TextLanguageId value);

  /// Shapes again on the next update, for a change the input can't see
  /// itself, like new system languages.
  void markShapeDirty();

  float paragraphSpacing() const { return m_paragraphSpacing; }
  void paragraphSpacing(float value);

  float selectionCornerRadius() const { return m_selectionCornerRadius; }
  void selectionCornerRadius(float value);

  bool separateSelectionText() const;
  void separateSelectionText(bool value);

  // When obscured, every code point displays as a bullet while the real
  // text stays intact for editing.
  bool obscured() const;
  void obscured(bool value);

  // Most code points insert() lets the text grow to, 0 for no limit. Only
  // edits are held to it: text set from outside and undo/redo are kept as
  // they are, even when they exceed it.
  uint32_t maxCharacters() const { return m_maxCharacters; }
  void maxCharacters(uint32_t value) { m_maxCharacters = value; }

  // Most displayed (wrapped) lines insert() lets the text grow to, 0 for no
  // limit. Held to edits only, like maxCharacters.
  uint32_t maxLines() const { return m_maxLines; }
  void maxLines(uint32_t value) { m_maxLines = value; }

  // Whether the layout hugs the text's width, so the field grows with its
  // text up to the width measure() was last offered: maxLines then wraps
  // there. Otherwise it wraps at maxWidth, where the text displays.
  bool hugsWidth() const;
  void hugsWidth(bool value);

  enum class Flags : uint16_t
  {
    none = 0,
    shapeDirty = 1 << 0,
    selectionDirty = 1 << 1,
    separateSelectionText = 1 << 2,
    measureDirty = 1 << 3,
    obscured = 1 << 4,
    // measure() has run, so the layout's width is known.
    measured = 1 << 5,
    hugsWidth = 1 << 6,
    // Shaped since update() last built the paths. Shaping clears
    // shapeDirty, and editing shapes on demand, so this is what tells
    // update() its paths are out of date.
    shaped = 1 << 7,
    // Shape the paragraphs again; shapeDirty alone only lays them out.
    paragraphsDirty = 1 << 8
  };

  Flags update(Factory* factory);

  // Builds text paths only for lines near [minY, maxY], rebuilding them when
  // the range leaves what was built. Returns true when it rebuilt them.
  // min > max builds every line.
  bool visibleRange(float minY, float maxY, Factory* factory);

  ShapePaintPath* textPath() { return &m_textPath; }
  ShapePaintPath* selectedTextPath() { return &m_selectedTextPath; }
  ShapePaintPath* cursorPath() { return &m_cursorPath; }
  ShapePaintPath* selectionPath() { return &m_selectionPath; }
  rcp<RenderPath> clipRenderPath() { return m_clipRenderPath; }

  /// Returns the bounds of the text object (helpful for aligning multiple
  /// text objects/procredurally drawn shapes).
  AABB bounds() const;
  CursorVisualPosition cursorVisualPosition(CursorPosition position)
  {
    return position.visualPosition(m_shape);
  }

  CursorVisualPosition cursorVisualPosition() const
  {
    return m_cursorVisualPosition;
  }

  // Replace the selection with text, truncated to what fits within
  // maxCharacters and maxLines. Returns false, changing nothing, when none
  // of it fits.
  bool insert(const std::string& text);
  bool insert(Unichar codePoint);
  void erase();
  void backspace(int32_t direction);

  void cursorLeft(
      CursorBoundary boundary = CursorBoundary::character,
      bool select = false);
  void cursorRight(
      CursorBoundary boundary = CursorBoundary::character,
      bool select = false);
  void cursorUp(bool select = false);
  void cursorDown(bool select = false);

  void moveCursorTo(Vec2D translation, bool select = false);
  const FullyShapedText& shape() const { return m_shape; }
  const Cursor cursor() const { return m_cursor; }
  void cursor(Cursor value);

  // Selects the word at the cursor.
  void selectWord();

  // Selects the entire text.
  void selectAll();

  // Collapses the cursor after the last character.
  void moveCursorToEnd();

  // Selects the visual line at the cursor.
  void selectLine();

  // Collapses the selection to the cursor's end position.
  void clearSelection();

  std::string text() const;
  // The text within the cursor's selection range (empty when the selection
  // is collapsed).
  std::string selectedText() const;
  void text(std::string value);
  void textPreserveCursor(std::string value);
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
    any = lowercase | uppercase | symbol | underscore | whitespace | lowercase

  };

private:
  void captureJournalEntry(Cursor cursor);
  uint32_t textIndex(CursorPosition position) const;
  uint32_t clusterLength(uint32_t index) const;
  void cursorHorizontal(int32_t offset, CursorBoundary boundary, bool select);
  static Delineator classify(Unichar codepoint);
  Delineator classify(CursorPosition position) const;

  // Hunt for a delineator bit maks from position in direction. Returns what
  // was actually found. The position will be updated to the location where
  // the Delineator was found.
  Delineator find(
      uint8_t delineatorMask,
      CursorPosition& position,
      int32_t direction) const;

  CursorPosition findPosition(
      uint8_t delineatorMask,
      const CursorPosition& position,
      int32_t direction) const;

  const OrderedLine* orderedLine(CursorPosition position) const;

  bool insertCodePoints(Span<const Unichar> codePoints);
  size_t fittingCount(Span<const Unichar> codePoints);
  uint32_t lineCountWith(
      Span<const Unichar> codePoints,
      size_t count,
      bool withRestOfParagraph);
  // Replaces count code points at start with added, for undo to take back.
  void replaceText(uint32_t start, uint32_t count, Span<const Unichar> added);
  void spliceText(uint32_t start, uint32_t count, Span<const Unichar> added);
  void textEdited(uint32_t start, uint32_t removed, uint32_t added);
  void reshapeAll();
  void ensureParagraphs();
  void ensureShape();
  // The position at a code point index. Its line is resolved now when the
  // text can be shaped, else left for update() to resolve once the font
  // arrives: a focus can select or place the caret before then.
  CursorPosition positionAtIndex(uint32_t codePointIndex);
  std::vector<Unichar>& shapeableText();
  void buildTextPaths(Factory* factory);
  void addLine(
      const OrderedLine& line,
      ShapePaintPath& textPath,
      ShapePaintPath* selectedTextPath);
  void keepLinePaths(size_t first, size_t end);
  void forgetLinePaths(size_t first, size_t end);
  void linesChanged(const FullyShapedText::LineChange& change);
  void computeVisualPositionFromCursor();
  void setTextPrivate(const std::string& value);
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
  std::vector<Unichar> m_obscuredText;

  FullyShapedText m_shape;
  // Only lineCountWith() uses it.
  std::unique_ptr<FullyShapedText> m_measuringShape;
  AABB m_measuredBounds;
  float m_lastMeasureMaxWidth = 0.0f;
  float m_lastMeasureMaxHeight = 0.0f;

  Flags m_flags = Flags::none;

  // Edits since the last shape: [m_editStart, m_editOldEnd) of the shaped
  // text is now [m_editStart, m_editNewEnd).
  bool m_reshapeAll = true;
  bool m_editPending = false;
  uint32_t m_editStart = 0;
  uint32_t m_editOldEnd = 0;
  uint32_t m_editNewEnd = 0;

  float m_paragraphSpacing = 0.0f;
  TextOrigin m_origin = TextOrigin::top;
  TextSizing m_sizing = TextSizing::autoWidth;
  TextOverflow m_overflow = TextOverflow::visible;
  TextAlign m_align = TextAlign::left;
  TextWrap m_wrap = TextWrap::wrap;
  float m_maxWidth = 0.0f;
  float m_maxHeight = 0.0f;
  float m_alignWidth = 0.0f;
  VerticalTextAlign m_verticalAlign = VerticalTextAlign::top;
  float m_alignHeight = 0.0f;
  rcp<RenderPath> m_clipRenderPath;
  float m_idealCursorX = -1.0f;

  // min > max: every line.
  float m_visibleMinY = 1.0f;
  float m_visibleMaxY = 0.0f;
  float m_builtMinY = 1.0f;
  float m_builtMaxY = 0.0f;

  // The outline of ordered line m_linePathsFirst + i, good while the line sits
  // at y, so an edit or a scroll builds only the lines it brings in.
  struct LinePath
  {
    ShapePaintPath path;
    float y = std::numeric_limits<float>::quiet_NaN();
  };
  std::vector<LinePath> m_linePaths;
  size_t m_linePathsFirst = 0;

  CursorVisualPosition m_cursorVisualPosition;
  std::vector<AABB> m_selectionRects;

  float m_selectionCornerRadius = 5.0f;
  uint32_t m_maxCharacters = 0;

  struct TextEdit
  {
    uint32_t start;
    std::vector<Unichar> removed;
    std::vector<Unichar> inserted;
  };
  struct JournalEntry
  {
    Cursor cursorFrom;
    Cursor cursorTo;
    std::vector<TextEdit> edits;
  };
  void revertEdits(const std::vector<TextEdit>& edits);
  void revertPendingEdits();
  std::vector<JournalEntry> m_journal;
  // Edits no journal entry holds yet.
  std::vector<TextEdit> m_pendingEdits;
  uint32_t m_journalIndex = 0;
  uint32_t m_maxLines = 0;
};

// Helper for performing bitwise and with Delineator.
inline uint8_t operator&(
    const RawTextInput::Delineator& delineator,
    const uint8_t mask)
{
  return (uint8_t)delineator & mask;
}

inline uint8_t operator~(const RawTextInput::Delineator& delineator)
{
  return ~(uint8_t)delineator;
}

inline RawTextInput::Delineator operator|(
    const RawTextInput::Delineator& a,
    const RawTextInput::Delineator& b)
{
  return (RawTextInput::Delineator)((uint8_t)a | (uint8_t)b);
}

inline RawTextInput::Delineator operator&(
    const RawTextInput::Delineator& a,
    const RawTextInput::Delineator& b)
{
  return (RawTextInput::Delineator)((uint8_t)a & (uint8_t)b);
}
} // namespace rive

#endif
#endif