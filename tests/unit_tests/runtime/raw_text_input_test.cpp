#ifdef WITH_RIVE_TEXT
#include "rive/text/cursor.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text_input.hpp"
#include "rive_testing.hpp"
#include "utils/no_op_factory.hpp"
using namespace rive;

TEST_CASE("cursor operators work", "[text_input]")
{
  CursorPosition a(0, 1);
  CursorPosition b(0, 4);
  CursorPosition c(0, 4);
  CHECK(a < b);
  CHECK(b > a);
  CHECK(c == b);
  CHECK(c != a);

  CursorPosition d(0, 1);
  d -= 1;
  CHECK(d.codePointIndex() == 0);
  d -= 1;
  // Still at 0, no overflow.
  CHECK(d.codePointIndex() == 0);
}

static rcp<Font> loadFont(const char* filename)
{
  FILE* fp = fopen(filename, "rb");

  fseek(fp, 0, SEEK_END);
  const size_t length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  std::vector<uint8_t> bytes(length);
  if (fread(bytes.data(), 1, length, fp) != length)
  {
    fclose(fp);
    return nullptr;
  }
  fclose(fp);

  return HBFont::Decode(bytes);
}

TEST_CASE(
    "text input decodes a cut short sequence inside its text",
    "[text_input]")
{
  RawTextInput textInput;
  textInput.text("a\xE2"
                 "b");
  CHECK(textInput.length() == 3);
  textInput.insert(std::string("\xF0", 1));
  CHECK(textInput.length() == 4);
  CHECK(
      textInput.text() == "\xEF\xBF\xBD"
                          "a\xEF\xBF\xBD"
                          "b");
}

TEST_CASE("cursor's visual position computes correctly", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  std::string
      defaultText = "this is some\nmultiline text input\nwith one final line\n";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);

  auto position = textInput.cursorVisualPosition(CursorPosition::zero());

  CHECK(position.found());
  CHECK(position.x() == 0.0f);
  CHECK(position.top() == 0.0f);
  CHECK(position.bottom() == Approx(87.11719f));

  position = textInput.cursorVisualPosition(CursorPosition(0, 1));

  CHECK(position.found());
  CHECK(position.x() == Approx(23.30859f));
  CHECK(position.top() == 0.0f);
  CHECK(position.bottom() == Approx(87.11719f));

  position = textInput.cursorVisualPosition(CursorPosition(0, 2));

  CHECK(position.found());
  CHECK(position.x() == Approx(65.17969f));
  CHECK(position.top() == 0.0f);
  CHECK(position.bottom() == Approx(87.11719f));

  // When we're passed the last character on the line we should still show the
  // caret on that same line.
  position = textInput.cursorVisualPosition(CursorPosition(0, 12));

  CHECK(position.found());
  CHECK(position.x() == Approx(396.0f));
  CHECK(position.top() == 0.0f);
  CHECK(position.bottom() == Approx(87.11719f));
}

TEST_CASE("moveCursorToEnd before the font arrives", "[text_input]")
{
  // A focus can land before the font does: the caret still goes after the
  // text, and its line resolves once the text can be shaped.
  RawTextInput textInput;
  textInput.text("hello");
  textInput.moveCursorToEnd();
  CHECK(textInput.cursor().isCollapsed());
  CHECK(textInput.cursor().end().codePointIndex() == 5);
  CHECK_FALSE(textInput.cursor().end().hasLineIndex());

  textInput.font(loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf"));
  NoOpFactory factory;
  textInput.update(&factory);
  CHECK(textInput.cursor().end().codePointIndex() == 5);
  CHECK(textInput.cursor().end().lineIndex() == 0);
}

TEST_CASE("selectAll before the font arrives", "[text_input]")
{
  // Tab or selectAllOnFocus can focus before the font arrives: the text is
  // still selected, and the lines resolve once it can be shaped.
  RawTextInput textInput;
  textInput.text("hello");
  textInput.selectAll();
  CHECK(textInput.selectedText() == "hello");
  CHECK_FALSE(textInput.cursor().start().hasLineIndex());
  CHECK_FALSE(textInput.cursor().end().hasLineIndex());

  textInput.font(loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf"));
  NoOpFactory factory;
  textInput.update(&factory);
  CHECK(textInput.selectedText() == "hello");
  CHECK(textInput.cursor().start().lineIndex() == 0);
  CHECK(textInput.cursor().end().lineIndex() == 0);
}

TEST_CASE("cursor is placed correctly with ltr paragraphs", "[text_input]")
{
  auto font = loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf");

  const float maxWidth = 500;
  RawTextInput textInput;
  std::string defaultText = "one two three four five";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(maxWidth);
  textInput.fontSize(72.0f);

  CHECK_AABB(textInput.bounds(), AABB());
  NoOpFactory factory;
  textInput.update(&factory);

  CHECK_AABB(textInput.bounds(), AABB(0, 0, 446.51953f, 216));
  CHECK_AABB(textInput.measure(500, 400), AABB(0, 0, 446.51953f, 216));
  CHECK(textInput.measureCount == 1);
  // measure count should still be one if we re-measured with same sizes.
  CHECK_AABB(textInput.measure(500, 400), AABB(0, 0, 446.51953f, 216));
  CHECK(textInput.measureCount == 1);
  CHECK_AABB(textInput.measure(400, 400), AABB(0, 0, 318.97266f, 324));
  CHECK(textInput.measureCount == 2);
  textInput.text("one two three four five six");
  CHECK_AABB(textInput.measure(400, 400), AABB(0, 0, 318.97266f, 324));
  CHECK(textInput.measureCount == 3);
  textInput.text("one two three four five");

  CHECK(textInput.shape().paragraphs().size() == 1);
  Paragraph& paragraph = textInput.shape().paragraphs()[0];
  CHECK(paragraph.baseDirection() == TextDirection::ltr);
  CHECK(textInput.shape().orderedLines().size() == 2);

  // Ensure that clicking beyond the bounds of each line places the cursor at
  // the begginging/end of the line.
  const OrderedLine& secondLine = textInput.shape().orderedLines()[1];

  CHECK(textInput.cursor().start().codePointIndex() == 0);
  // Click to the left of the whole line of text.
  textInput.moveCursorTo(Vec2D(-20.0f, secondLine.y()));

  CHECK(
      textInput.cursor().start().codePointIndex() ==
      secondLine.firstCodePointIndex(textInput.shape().glyphLookup()));

  // Click to the right of the whole line of text.
  textInput.moveCursorTo(Vec2D(maxWidth + 20.0f, secondLine.y()));
  CHECK(
      textInput.cursor().start().codePointIndex() ==
      secondLine.lastCodePointIndex(textInput.shape().glyphLookup()));
}

TEST_CASE("cursor is placed correctly with rtl paragraphs", "[text_input]")
{
  auto font = loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf");

  const float maxWidth = 500;
  RawTextInput textInput;
  std::string defaultText = "اربك تكست هو اول موقع يسمح لزواره";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(maxWidth);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);

  CHECK(textInput.shape().paragraphs().size() == 1);
  Paragraph& paragraph = textInput.shape().paragraphs()[0];
  CHECK(paragraph.baseDirection() == TextDirection::rtl);
  CHECK(textInput.shape().orderedLines().size() == 3);

  // Ensure that clicking beyond the bounds of each line places the cursor at
  // the begginging/end of the line.
  const OrderedLine& secondLine = textInput.shape().orderedLines()[1];

  CHECK(textInput.cursor().start().codePointIndex() == 0);
  // Click to the left of the whole line of text.
  textInput.moveCursorTo(Vec2D(-20.0f, secondLine.y()));

  CHECK(
      textInput.cursor().start().codePointIndex() ==
      secondLine.firstCodePointIndex(textInput.shape().glyphLookup()));

  // Click to the right of the whole line of text.
  textInput.moveCursorTo(Vec2D(maxWidth + 20.0f, secondLine.y()));
  CHECK(
      textInput.cursor().start().codePointIndex() ==
      secondLine.lastCodePointIndex(textInput.shape().glyphLookup()));
}

TEST_CASE(
    "cursor is placed correctly with mixed bidi paragraphs",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf");

  const float maxWidth = 500;
  RawTextInput textInput;
  std::string defaultText =
      "one two three four اربك تكست هو اول موقع يسمح لزواره الكرام بتحويل";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(maxWidth);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);

  CHECK(textInput.shape().paragraphs().size() == 1);
  Paragraph& paragraph = textInput.shape().paragraphs()[0];
  CHECK(paragraph.baseDirection() == TextDirection::ltr);
  CHECK(textInput.shape().orderedLines().size() == 5);

  // Ensure that clicking beyond the bounds of each line places the cursor at
  // the begginging/end of the line.
  const OrderedLine& secondLine = textInput.shape().orderedLines()[1];

  CHECK(textInput.cursor().start().codePointIndex() == 0);
  // Click to the left of the whole line of text.
  textInput.moveCursorTo(Vec2D(-20.0f, secondLine.y()));

  CHECK(
      textInput.cursor().start().codePointIndex() ==
      secondLine.firstCodePointIndex(textInput.shape().glyphLookup()));

  // Click to the right of the whole line of text.
  textInput.moveCursorTo(Vec2D(maxWidth + 20.0f, secondLine.y()));
  CHECK(
      textInput.cursor().start().codePointIndex() ==
      secondLine.lastCodePointIndex(textInput.shape().glyphLookup()));
}

TEST_CASE("cursor moves correctly", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  std::string
      defaultText = "this is some\nmultiline text input\nwith one final line";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);

  textInput.cursorRight();
  textInput.update(&factory);
  CHECK(textInput.cursor().start() == CursorPosition(0, 1));

  for (int i = 0; i < 12; i++)
  {
    textInput.cursorRight();
  }
  textInput.update(&factory);
  CHECK(textInput.cursor().start() == CursorPosition(1, 13));
  textInput.cursorRight();
  textInput.cursorRight();
  textInput.update(&factory);
  CHECK(textInput.cursor().start() == CursorPosition(1, 15));

  // Up once takes us to the previous line and the closest glyph's codepoint.
  textInput.cursorUp();
  textInput.update(&factory);
  CHECK(textInput.cursor().start() == CursorPosition(0, 4));

  // Up again goes to the start of the text.
  textInput.cursorUp();
  textInput.update(&factory);
  CHECK(textInput.cursor().start() == CursorPosition(0, 0));

  textInput.cursorRight();
  textInput.cursorRight();
  textInput.cursorRight();
  textInput.update(&factory);
  CHECK(textInput.cursor().start() == CursorPosition(0, 3));

  textInput.cursorDown();
  textInput.update(&factory);
  CHECK(textInput.cursor().start() == CursorPosition(1, 14));

  // Next cursor down takes us to the closest codePoint on the last line.
  textInput.cursorDown();
  textInput.update(&factory);
  CHECK(textInput.cursor().start() == CursorPosition(2, 36));

  // Next cursor down should reach the end of the last line since we're
  // already on the last line.
  textInput.cursorDown();
  textInput.update(&factory);
  CHECK(textInput.cursor().start() == CursorPosition(2, 53));
}

TEST_CASE("text inputs correctly", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  std::string defaultText = "hello ";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);
  CHECK(textInput.text() == "hello ");
  // Quickly goes to end.
  textInput.cursorDown();
  textInput.insert("world");
  CHECK(textInput.text() == "hello world");

  textInput.text("foo");
  CHECK(textInput.text() == "foo");
}

TEST_CASE("cursor home/end works", "[text_input]")
{
  auto font = loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf");

  const float maxWidth = 500;
  RawTextInput textInput;
  std::string defaultText = "one two three four five";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(maxWidth);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);

  CHECK(textInput.shape().orderedLines().size() == 2);
  textInput.cursorDown();
  textInput.update(&factory);
  CHECK(textInput.cursor().start() == CursorPosition(1, 14));

  textInput.cursorRight(CursorBoundary::line, false);
  textInput.update(&factory);
  CHECK(textInput.cursor().start() == CursorPosition(1, 23));

  textInput.cursorLeft(CursorBoundary::line, false);
  textInput.update(&factory);
  CHECK(textInput.cursor().start() == CursorPosition(1, 14));
}

TEST_CASE("cursor word movement works", "[text_input]")
{
  auto font = loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf");

  const float maxWidth = 500;
  RawTextInput textInput;
  std::string defaultText = "one two three fo4ur five";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(maxWidth);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);

  // "|one two three fo4ur five"
  CHECK(textInput.cursor().start().codePointIndex() == 0);
  textInput.cursorRight(CursorBoundary::word);
  textInput.update(&factory);
  // "one| two three fo4ur five"
  CHECK(textInput.cursor().start().codePointIndex() == 3);
  textInput.cursorRight(CursorBoundary::word);
  textInput.update(&factory);
  // "one two| three fo4ur five"
  CHECK(textInput.cursor().start().codePointIndex() == 7);
  textInput.cursorLeft(CursorBoundary::word);
  textInput.update(&factory);
  // "one |two three fo4ur five"
  CHECK(textInput.cursor().start().codePointIndex() == 4);
  textInput.cursorLeft(CursorBoundary::word);
  textInput.update(&factory);
  // "|one two three fo4ur five"
  CHECK(textInput.cursor().start().codePointIndex() == 0);
  textInput.cursorRight(CursorBoundary::word);
  textInput.cursorRight(CursorBoundary::word);
  textInput.cursorRight(CursorBoundary::word);
  textInput.cursorRight(CursorBoundary::word);
  textInput.update(&factory);
  // "one two three fo4ur| five"
  CHECK(textInput.cursor().start().codePointIndex() == 19);
  textInput.cursorLeft(CursorBoundary::character);
  textInput.cursorLeft(CursorBoundary::character);
  textInput.update(&factory);
  // "one two three fo4|ur five"
  CHECK(textInput.cursor().start().codePointIndex() == 17);
  textInput.cursorLeft(CursorBoundary::word);
  textInput.update(&factory);
  // "one two three |fo4ur five"
  CHECK(textInput.cursor().start().codePointIndex() == 14);
  textInput.cursorRight(CursorBoundary::character);
  textInput.cursorRight(CursorBoundary::character);
  textInput.update(&factory);
  // "one two three fo|4ur five"
  CHECK(textInput.cursor().start().codePointIndex() == 16);
  textInput.cursorRight(CursorBoundary::word);
  textInput.update(&factory);
  // "one two three fo4ur| five"
  CHECK(textInput.cursor().start().codePointIndex() == 19);
}

TEST_CASE("cursor sub-word movement works", "[text_input]")
{
  auto font = loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf");

  const float maxWidth = 500;
  RawTextInput textInput;
  std::string defaultText = "oneTwo threeFo+ur fi--ve";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(maxWidth);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);

  // "|oneTwo threeFo+ur fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 0);
  textInput.cursorRight(CursorBoundary::subWord);
  textInput.update(&factory);
  // "one|Two threeFo+ur fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 3);
  textInput.cursorRight(CursorBoundary::subWord);
  textInput.update(&factory);
  // "oneTwo| threeFo+ur fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 6);
  textInput.cursorRight(CursorBoundary::subWord);
  textInput.update(&factory);
  // "oneTwo three|Fo+ur fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 12);
  textInput.cursorRight(CursorBoundary::subWord);
  textInput.update(&factory);
  // "oneTwo threeFo|+ur fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 14);
  textInput.cursorRight(CursorBoundary::subWord);
  textInput.update(&factory);
  // "oneTwo threeFo+|ur fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 15);
  textInput.cursorRight(CursorBoundary::subWord);
  textInput.update(&factory);
  // "oneTwo threeFo+ur| fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 17);
  textInput.cursorLeft(CursorBoundary::subWord);
  textInput.update(&factory);
  // "oneTwo threeFo+|ur fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 15);
  textInput.cursorLeft(CursorBoundary::subWord);
  textInput.update(&factory);
  // "oneTwo threeFo|+ur fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 14);
  textInput.cursorLeft(CursorBoundary::subWord);
  textInput.update(&factory);
  // "oneTwo three|Fo+ur fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 12);
  textInput.cursorLeft(CursorBoundary::subWord);
  textInput.update(&factory);
  // "oneTwo |threeFo+ur fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 7);
  textInput.cursorRight(CursorBoundary::word);
  textInput.update(&factory);
  // "oneTwo threeFo|+ur fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 14);
  textInput.cursorRight(CursorBoundary::word);
  textInput.update(&factory);
  // "oneTwo threeFo+|ur fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 15);
  textInput.cursorRight(CursorBoundary::word);
  textInput.update(&factory);
  // "oneTwo threeFo+ur| fi--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 17);
  textInput.cursorRight(CursorBoundary::subWord);
  textInput.update(&factory);
  // "oneTwo threeFo+ur fi|--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 20);
  textInput.cursorRight(CursorBoundary::subWord);
  textInput.update(&factory);
  // "oneTwo threeFo+ur fi--|ve"
  CHECK(textInput.cursor().start().codePointIndex() == 22);
  textInput.cursorLeft(CursorBoundary::subWord);
  textInput.update(&factory);
  // "oneTwo threeFo+ur fi|--ve"
  CHECK(textInput.cursor().start().codePointIndex() == 20);
}

TEST_CASE("cursor skips multi-codepoint glyphs", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  // "cafe\u0301s" = "cafés" where é = e + combining acute accent (2
  // codepoints, 1 glyph)
  // Indices: c=0 a=1 f=2 e=3 \u0301=4 s=5
  std::string defaultText = "caf\xC3\xA9s"; // UTF-8 for café with precomposed é
  // Actually we need the decomposed form: e + combining acute accent
  // e = 0x65, combining acute = 0xCC 0x81 in UTF-8
  defaultText = "cafe\xCC\x81s";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);

  // Move right: c(0) -> a(1) -> f(2) -> e(3) -> s(5) (should skip index 4)
  textInput.cursorRight();
  textInput.update(&factory);
  CHECK(textInput.cursor().start().codePointIndex() == 1);

  textInput.cursorRight();
  textInput.update(&factory);
  CHECK(textInput.cursor().start().codePointIndex() == 2);

  textInput.cursorRight();
  textInput.update(&factory);
  CHECK(textInput.cursor().start().codePointIndex() == 3);

  textInput.cursorRight();
  textInput.update(&factory);
  // Should skip index 4 (combining accent) and land on 5
  CHECK(textInput.cursor().start().codePointIndex() == 5);
}

TEST_CASE("cursor left skips multi-codepoint glyphs", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  std::string defaultText = "cafe\xCC\x81s";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);

  // Go to end
  textInput.cursorDown();
  textInput.update(&factory);

  // Move left from end(6) -> s(5) -> é(3) (skip 4) -> f(2)
  textInput.cursorLeft();
  textInput.update(&factory);
  CHECK(textInput.cursor().start().codePointIndex() == 5);

  textInput.cursorLeft();
  textInput.update(&factory);
  // Should skip index 4 and land on 3
  CHECK(textInput.cursor().start().codePointIndex() == 3);

  textInput.cursorLeft();
  textInput.update(&factory);
  CHECK(textInput.cursor().start().codePointIndex() == 2);
}

TEST_CASE("backspace deletes entire multi-codepoint glyph", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  std::string defaultText = "cafe\xCC\x81s";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);

  // Move cursor to after the é (before 's')
  textInput.cursorDown();
  textInput.update(&factory);
  textInput.cursorLeft(); // at 's' = index 5
  textInput.update(&factory);
  CHECK(textInput.cursor().start().codePointIndex() == 5);

  // Backspace should delete both codepoints of é (indices 3 and 4)
  textInput.backspace(-1);
  textInput.update(&factory);
  CHECK(textInput.text() == "cafs");
  CHECK(textInput.cursor().start().codePointIndex() == 3);
}

TEST_CASE("delete forward removes entire multi-codepoint glyph", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  std::string defaultText = "cafe\xCC\x81s";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);

  // Move cursor to before the é (after 'f') = index 3
  textInput.cursorRight();
  textInput.cursorRight();
  textInput.cursorRight();
  textInput.update(&factory);
  CHECK(textInput.cursor().start().codePointIndex() == 3);

  // Delete forward should remove both codepoints of é
  textInput.backspace(1);
  textInput.update(&factory);
  CHECK(textInput.text() == "cafs");
}

#define CHECK_CURSOR(A, START, END)                                            \
  CHECK(A.start().codePointIndex() == START);                                  \
  CHECK(A.end().codePointIndex() == END)

TEST_CASE("word selection works", "[text_input]")
{
  auto font = loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf");

  const float maxWidth = 500;
  RawTextInput textInput;
  std::string defaultText = "oneTwo three == four";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(maxWidth);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);

  textInput.selectWord();
  CHECK_CURSOR(textInput.cursor(), 0, 6);

  textInput.cursor(Cursor::collapsed(CursorPosition(9)));
  textInput.selectWord();
  CHECK_CURSOR(textInput.cursor(), 7, 12);

  // Right edge of word selects word before it ("three")
  textInput.cursor(Cursor::collapsed(CursorPosition(12)));
  textInput.selectWord();
  CHECK_CURSOR(textInput.cursor(), 7, 12);

  textInput.cursor(Cursor::collapsed(CursorPosition(14)));
  textInput.selectWord();
  CHECK_CURSOR(textInput.cursor(), 13, 15);
}

TEST_CASE("text input journal works", "[text_input]")
{
  auto font = loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf");

  const float maxWidth = 500;
  RawTextInput textInput;
  std::string defaultText = "oneTwo";
  textInput.insert(defaultText);
  textInput.cursor(Cursor::zero());
  textInput.font(font);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(maxWidth);
  textInput.fontSize(72.0f);
  NoOpFactory factory;
  textInput.update(&factory);

  textInput.cursorRight();
  textInput.cursorRight();
  textInput.cursorRight();

  textInput.insert(" ");
  textInput.insert("2");
  textInput.insert(" ");
  textInput.update(&factory);
  CHECK(textInput.text() == "one 2 Two");
  CHECK_CURSOR(textInput.cursor(), 6, 6);

  textInput.undo();
  CHECK(textInput.text() == "one 2Two");
  CHECK_CURSOR(textInput.cursor(), 5, 5);

  textInput.undo();
  CHECK(textInput.text() == "one Two");
  CHECK_CURSOR(textInput.cursor(), 4, 4);

  textInput.undo();
  CHECK(textInput.text() == "oneTwo");
  CHECK_CURSOR(textInput.cursor(), 3, 3);

  textInput.redo();
  CHECK(textInput.text() == "one Two");
  CHECK_CURSOR(textInput.cursor(), 4, 4);

  textInput.insert("X");
  CHECK(textInput.text() == "one XTwo");
  CHECK_CURSOR(textInput.cursor(), 5, 5);

  // Redo does nothing as stack has been cleared by previous insertion
  textInput.redo();
  CHECK(textInput.text() == "one XTwo");
  CHECK_CURSOR(textInput.cursor(), 5, 5);

  // Undo still works, however.
  textInput.undo();
  CHECK(textInput.text() == "one Two");
  CHECK_CURSOR(textInput.cursor(), 4, 4);

  textInput.cursorRight(CursorBoundary::character, true);
  textInput.cursorRight(CursorBoundary::character, true);
  textInput.cursorRight(CursorBoundary::character, true);
  CHECK(textInput.text() == "one Two");
  CHECK_CURSOR(textInput.cursor(), 4, 7);
  textInput.insert("2");
  CHECK(textInput.text() == "one 2");
  CHECK_CURSOR(textInput.cursor(), 5, 5);

  textInput.undo();
  CHECK(textInput.text() == "one Two");
  CHECK_CURSOR(textInput.cursor(), 4, 7);
}

TEST_CASE(
    "moving left from a selection lands before its first character",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf");

  RawTextInput textInput;
  textInput.insert("hello world");
  textInput.font(font);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(500.0f);
  textInput.fontSize(72.0f);

  NoOpFactory factory;
  textInput.update(&factory);

  // Forward selection: "he[llo] world" collapses to "h|ello world".
  textInput.cursor(Cursor(CursorPosition(0, 2), CursorPosition(0, 5)));
  textInput.cursorLeft();
  CHECK_CURSOR(textInput.cursor(), 1, 1);

  // Backward selection behaves the same, the leading edge is what matters.
  textInput.cursor(Cursor(CursorPosition(0, 5), CursorPosition(0, 2)));
  textInput.cursorLeft();
  CHECK_CURSOR(textInput.cursor(), 1, 1);

  // Nothing to the left of the selection, so we stop at the beginning.
  textInput.cursor(Cursor(CursorPosition(0, 0), CursorPosition(0, 5)));
  textInput.cursorLeft();
  CHECK_CURSOR(textInput.cursor(), 0, 0);

  // Word moves start from the leading edge too: "hello [wor]ld" lands at the
  // start of "hello".
  textInput.cursor(Cursor(CursorPosition(0, 6), CursorPosition(0, 9)));
  textInput.cursorLeft(CursorBoundary::word);
  CHECK_CURSOR(textInput.cursor(), 0, 0);

  // Shift+left still extends from the active end.
  textInput.cursor(Cursor(CursorPosition(0, 2), CursorPosition(0, 5)));
  textInput.cursorLeft(CursorBoundary::character, true);
  CHECK_CURSOR(textInput.cursor(), 2, 4);

  // Moving right is unchanged, it collapses off the active end.
  textInput.cursor(Cursor(CursorPosition(0, 2), CursorPosition(0, 5)));
  textInput.cursorRight();
  CHECK_CURSOR(textInput.cursor(), 6, 6);
}

TEST_CASE("clearSelection collapses to the selection end", "[text_input]")
{
  RawTextInput textInput;
  textInput.insert("hello world");

  textInput.cursor(Cursor(CursorPosition(2), CursorPosition(7)));
  textInput.clearSelection();
  CHECK(textInput.cursor().isCollapsed());
  CHECK_CURSOR(textInput.cursor(), 7, 7);

  // No-op when already collapsed.
  textInput.clearSelection();
  CHECK_CURSOR(textInput.cursor(), 7, 7);
}

// Builds a single line input sized like a field of alignWidth wide.
static void makeAlignedInput(
    RawTextInput& textInput,
    rcp<Font> font,
    TextAlign align,
    float alignWidth,
    const char* text = "hello")
{
  textInput.font(font);
  textInput.fontSize(72.0f);
  textInput.insert(text);
  textInput.cursor(Cursor::zero());
  textInput.align(align);
  textInput.alignWidth(alignWidth);
}

TEST_CASE("a single line input aligns within its align width", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;
  const float alignWidth = 800.0f;

  // Measure the natural width of the text so the expectations below don't
  // depend on exact Inter metrics.
  float naturalWidth;
  float leftEndX;
  {
    RawTextInput textInput;
    makeAlignedInput(textInput, font, TextAlign::left, alignWidth);
    textInput.update(&factory);
    naturalWidth = textInput.bounds().width();
    CHECK(naturalWidth > 0.0f);
    CHECK(naturalWidth < alignWidth);

    // Left align is unaffected by the align width.
    CHECK(textInput.cursorVisualPosition(CursorPosition::zero()).x() == 0.0f);
    CHECK(textInput.bounds().minX == 0.0f);
    leftEndX = textInput.cursorVisualPosition(CursorPosition(0, 5)).x();
    CHECK(leftEndX == Approx(naturalWidth));
  }

  {
    RawTextInput textInput;
    makeAlignedInput(textInput, font, TextAlign::right, alignWidth);
    textInput.update(&factory);

    CHECK(
        textInput.cursorVisualPosition(CursorPosition::zero()).x() ==
        Approx(alignWidth - naturalWidth));
    CHECK(
        textInput.cursorVisualPosition(CursorPosition(0, 5)).x() ==
        Approx(alignWidth));
    // Bounds follow the glyphs, and the span is unchanged.
    CHECK(textInput.bounds().minX == Approx(alignWidth - naturalWidth));
    CHECK(textInput.bounds().maxX == Approx(alignWidth));
    CHECK(textInput.bounds().width() == Approx(naturalWidth));
  }

  {
    RawTextInput textInput;
    makeAlignedInput(textInput, font, TextAlign::center, alignWidth);
    textInput.update(&factory);

    float expectedStart = (alignWidth - naturalWidth) / 2.0f;
    CHECK(
        textInput.cursorVisualPosition(CursorPosition::zero()).x() ==
        Approx(expectedStart));
    CHECK(
        textInput.cursorVisualPosition(CursorPosition(0, 5)).x() ==
        Approx(expectedStart + naturalWidth));
    CHECK(textInput.bounds().minX == Approx(expectedStart));
    CHECK(textInput.bounds().width() == Approx(naturalWidth));
  }
}

TEST_CASE(
    "alignment falls back to the left when the text overflows",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;

  // An align width narrower than the text: alignment must not shift the text,
  // so the horizontal scrolling behavior stays intact.
  for (TextAlign align : {TextAlign::left, TextAlign::right, TextAlign::center})
  {
    RawTextInput textInput;
    makeAlignedInput(textInput, font, align, 10.0f);
    textInput.update(&factory);

    CHECK(textInput.cursorVisualPosition(CursorPosition::zero()).x() == 0.0f);
    CHECK(textInput.bounds().minX == 0.0f);
  }

  // A zero align width (the default) is equally a no-op.
  RawTextInput textInput;
  makeAlignedInput(textInput, font, TextAlign::right, 0.0f);
  textInput.update(&factory);
  CHECK(textInput.cursorVisualPosition(CursorPosition::zero()).x() == 0.0f);
}

TEST_CASE("hit testing round trips through alignment", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;
  const float alignWidth = 800.0f;

  for (TextAlign align : {TextAlign::left, TextAlign::right, TextAlign::center})
  {
    RawTextInput textInput;
    makeAlignedInput(textInput, font, align, alignWidth);
    textInput.update(&factory);

    for (uint32_t index = 0; index <= 5; index++)
    {
      auto position = textInput.cursorVisualPosition(CursorPosition(0, index));
      REQUIRE(position.found());
      float y = (position.top() + position.bottom()) / 2.0f;
      textInput.moveCursorTo(Vec2D(position.x(), y), false);
      CHECK(textInput.cursor().end().codePointIndex() == index);
    }
  }
}

TEST_CASE("multiline alignment offsets each line independently", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;
  const float maxWidth = 900.0f;

  // Two lines of clearly different widths.
  auto lineStartX = [&](TextAlign align, uint32_t lineIndex) {
    RawTextInput textInput;
    textInput.font(font);
    textInput.fontSize(72.0f);
    textInput.sizing(TextSizing::autoHeight);
    textInput.maxWidth(maxWidth);
    textInput.align(align);
    textInput.insert("hello\nlonger line here");
    textInput.update(&factory);
    REQUIRE(textInput.shape().lineCount() > lineIndex);
    return textInput.shape().orderedLines()[lineIndex].glyphLine().startX;
  };

  CHECK(lineStartX(TextAlign::left, 0) == 0.0f);
  CHECK(lineStartX(TextAlign::left, 1) == 0.0f);

  // The shorter line is pushed further right than the longer one.
  float rightShort = lineStartX(TextAlign::right, 0);
  float rightLong = lineStartX(TextAlign::right, 1);
  CHECK(rightShort > rightLong);
  CHECK(rightLong > 0.0f);

  float centerShort = lineStartX(TextAlign::center, 0);
  float centerLong = lineStartX(TextAlign::center, 1);
  CHECK(centerShort > centerLong);
  CHECK(centerShort == Approx(rightShort / 2.0f));
  CHECK(centerLong == Approx(rightLong / 2.0f));
}

TEST_CASE("an input aligns vertically within its align height", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;
  const float alignHeight = 500.0f;

  // The height of the text itself, so the expectations below don't depend on
  // Inter's exact metrics.
  float naturalHeight;
  float topMinY;
  float topCaretY;
  {
    RawTextInput textInput;
    makeAlignedInput(textInput, font, TextAlign::left, 0.0f);
    textInput.alignHeight(alignHeight);
    textInput.update(&factory);
    naturalHeight = textInput.bounds().height();
    topMinY = textInput.bounds().minY;
    topCaretY = textInput.cursorVisualPosition(CursorPosition(0, 0)).top();
    CHECK(naturalHeight < alignHeight);
  }

  // Top is the default and must not move the text at all.
  {
    RawTextInput textInput;
    makeAlignedInput(textInput, font, TextAlign::left, 0.0f);
    textInput.alignHeight(alignHeight);
    textInput.verticalAlign(VerticalTextAlign::top);
    textInput.update(&factory);
    CHECK(textInput.shape().verticalOffset() == 0.0f);
    CHECK(textInput.bounds().minY == Approx(topMinY));
  }

  {
    RawTextInput textInput;
    makeAlignedInput(textInput, font, TextAlign::left, 0.0f);
    textInput.alignHeight(alignHeight);
    textInput.verticalAlign(VerticalTextAlign::middle);
    textInput.update(&factory);
    CHECK(
        textInput.shape().verticalOffset() ==
        Approx((alignHeight - naturalHeight) / 2.0f));
    CHECK(
        textInput.bounds().minY ==
        Approx(topMinY + (alignHeight - naturalHeight) / 2.0f));
    CHECK(textInput.bounds().height() == Approx(naturalHeight));
    // The caret follows the text down by the same amount.
    CHECK(
        textInput.cursorVisualPosition(CursorPosition(0, 0)).top() -
            topCaretY ==
        Approx((alignHeight - naturalHeight) / 2.0f));
  }

  {
    RawTextInput textInput;
    makeAlignedInput(textInput, font, TextAlign::left, 0.0f);
    textInput.alignHeight(alignHeight);
    textInput.verticalAlign(VerticalTextAlign::bottom);
    textInput.update(&factory);
    CHECK(
        textInput.shape().verticalOffset() ==
        Approx(alignHeight - naturalHeight));
    CHECK(textInput.bounds().maxY == Approx(topMinY + alignHeight));
  }

  // Text taller than the field stays at the top, so vertical scrolling is
  // left alone -- the mirror of the horizontal overflow fallback.
  {
    RawTextInput textInput;
    makeAlignedInput(textInput, font, TextAlign::left, 0.0f);
    textInput.alignHeight(10.0f);
    textInput.verticalAlign(VerticalTextAlign::bottom);
    textInput.update(&factory);
    CHECK(textInput.shape().verticalOffset() == 0.0f);
    CHECK(textInput.bounds().minY == Approx(topMinY));
  }
}

// A click lands on the same character the caret reports, whatever the vertical
// alignment -- hit testing has to move with the text.
TEST_CASE("vertical alignment round trips through hit testing", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;

  RawTextInput textInput;
  makeAlignedInput(textInput, font, TextAlign::left, 0.0f);
  textInput.alignHeight(500.0f);
  textInput.verticalAlign(VerticalTextAlign::middle);
  textInput.update(&factory);

  auto position = textInput.cursorVisualPosition(CursorPosition(0, 3));
  CHECK(position.found());
  textInput.moveCursorTo(
      Vec2D(position.x(), (position.top() + position.bottom()) / 2.0f));
  CHECK(textInput.cursor().end().codePointIndex() == 3);
}

// A multiline input hugs its widest wrapped line, so the width it wraps at is
// the text's own width, not the field's. Alignment still has to happen within
// the field -- otherwise the lines only align relative to each other and the
// block stays pinned left.
TEST_CASE(
    "multiline alignment uses the align width, not the wrap width",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;
  const float fieldWidth = 900.0f;

  auto build = [&](RawTextInput& textInput, TextAlign align) {
    textInput.font(font);
    textInput.fontSize(72.0f);
    textInput.sizing(TextSizing::autoHeight);
    textInput.align(align);
    textInput.insert("hello\nlonger line here");
    // Measure against the space available, as the layout does.
    textInput.maxWidth(fieldWidth);
    textInput.update(&factory);
    // Hug: wrap at the width the text actually took, the way the layout
    // hands it back to us.
    textInput.maxWidth(textInput.bounds().width());
    textInput.alignWidth(fieldWidth);
    textInput.update(&factory);
  };

  auto startX = [&](const RawTextInput& textInput, uint32_t lineIndex) {
    return textInput.shape().orderedLines()[lineIndex].glyphLine().startX;
  };

  float textWidth;
  {
    RawTextInput textInput;
    build(textInput, TextAlign::left);
    textWidth = textInput.bounds().width();
    REQUIRE(textInput.shape().lineCount() == 2);
    CHECK(textWidth < fieldWidth);
    CHECK(startX(textInput, 0) == 0.0f);
    CHECK(startX(textInput, 1) == 0.0f);
  }

  {
    RawTextInput textInput;
    build(textInput, TextAlign::right);
    // The longest line ends at the field's right edge, not at the text's.
    CHECK(startX(textInput, 1) == Approx(fieldWidth - textWidth));
    // And the short line is pushed further still.
    CHECK(startX(textInput, 0) > startX(textInput, 1));
  }

  {
    RawTextInput textInput;
    build(textInput, TextAlign::center);
    CHECK(startX(textInput, 1) == Approx((fieldWidth - textWidth) / 2.0f));
    CHECK(startX(textInput, 0) > startX(textInput, 1));
  }
}

TEST_CASE("obscured input shapes as bullets over intact text", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.insert("ab\ncd");

  NoOpFactory factory;
  textInput.update(&factory);
  REQUIRE(textInput.shape().paragraphs().size() == 2);

  textInput.obscured(true);
  CHECK(textInput.obscured());
  textInput.update(&factory);

  // Newlines mask too, so nothing of the text's structure shows.
  REQUIRE(textInput.shape().paragraphs().size() == 1);
  const GlyphRun& run = textInput.shape().paragraphs()[0].runs[0];
  REQUIRE(run.glyphs.size() >= 5);
  for (size_t i = 1; i < 5; i++)
  {
    CHECK(run.glyphs[i] == run.glyphs[0]);
  }

  // The real text is untouched and editing still works on it.
  CHECK(textInput.text() == "ab\ncd");
  textInput.insert('!');
  CHECK(textInput.text() == "ab\ncd!");
  textInput.backspace(-1);
  textInput.backspace(-1);
  CHECK(textInput.text() == "ab\nc");

  textInput.obscured(false);
  textInput.update(&factory);
  CHECK(textInput.shape().paragraphs().size() == 2);
}

TEST_CASE("obscured input treats the field as one word", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.insert("ab cd");
  textInput.obscured(true);

  NoOpFactory factory;
  textInput.update(&factory);

  // Double-click inside the field selects the whole run of bullets.
  textInput.cursor(Cursor::collapsed(CursorPosition(0, 3)));
  textInput.selectWord();
  CHECK(textInput.selectedText() == "ab cd");

  // A word jump crosses the whole field, the space stays hidden.
  textInput.cursor(Cursor::zero());
  textInput.cursorRight(CursorBoundary::word);
  CHECK(textInput.cursor().end().codePointIndex() == 5);
}

TEST_CASE("obscured toggle re-resolves cursor lines", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.insert("ab\ncd");

  NoOpFactory factory;

  // Resolve the caret on the second line.
  textInput.cursor(Cursor::collapsed(CursorPosition(4)));
  textInput.update(&factory);
  REQUIRE(textInput.cursor().end().lineIndex() == 1);
  REQUIRE(textInput.cursorVisualPosition().found());

  // Obscuring collapses to one line; the caret must follow.
  textInput.obscured(true);
  textInput.update(&factory);
  CHECK(textInput.cursor().end().lineIndex() == 0);
  CHECK(textInput.cursor().end().codePointIndex() == 4);
  CHECK(textInput.cursorVisualPosition().found());

  // And back again.
  textInput.obscured(false);
  textInput.update(&factory);
  CHECK(textInput.cursor().end().lineIndex() == 1);
  CHECK(textInput.cursorVisualPosition().found());
}

TEST_CASE("obscured toggle re-resolves journaled cursor lines", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.insert("ab\ncd");

  NoOpFactory factory;

  // Edit with a caret resolved on the second line, journaling that cursor.
  textInput.cursor(Cursor::collapsed(CursorPosition(4)));
  textInput.update(&factory);
  REQUIRE(textInput.cursor().end().lineIndex() == 1);
  textInput.insert('!');
  textInput.update(&factory);
  CHECK(textInput.text() == "ab\nc!d");

  // Undo under obscuring restores a journal cursor; its line must resolve
  // against the masked single-line layout.
  textInput.obscured(true);
  textInput.update(&factory);
  textInput.undo();
  textInput.update(&factory);
  CHECK(textInput.text() == "ab\ncd");
  CHECK(textInput.cursor().end().lineIndex() == 0);
  CHECK(textInput.cursorVisualPosition().found());
}

TEST_CASE("maxCharacters caps typing and truncates pastes", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.maxCharacters(5);
  CHECK(textInput.maxCharacters() == 5);

  CHECK(textInput.insert("abc"));
  CHECK(textInput.text() == "abc");

  // A paste keeps only what fits.
  CHECK(textInput.insert("defgh"));
  CHECK(textInput.text() == "abcde");
  CHECK(textInput.cursor().end().codePointIndex() == 5);

  // Full: nothing more goes in.
  CHECK_FALSE(textInput.insert('x'));
  CHECK_FALSE(textInput.insert("xyz"));
  CHECK(textInput.text() == "abcde");

  // Deleting makes room again.
  textInput.update(&factory);
  textInput.backspace(-1);
  CHECK(textInput.text() == "abcd");
  CHECK(textInput.insert('x'));
  CHECK(textInput.text() == "abcdx");

  // Replacing a selection counts the selected code points as free.
  textInput.cursor(Cursor(CursorPosition(0), CursorPosition(2)));
  CHECK(textInput.insert("XYZ"));
  CHECK(textInput.text() == "XYcdx");

  // Code points, not bytes: each of these is several UTF-8 bytes.
  RawTextInput unicode;
  unicode.maxCharacters(3);
  CHECK(unicode.insert("\xC3\xA9\xE2\x82\xAC\xF0\x9F\x99\x82\xC3\xA9"));
  CHECK(unicode.length() == 3);
  CHECK(unicode.text() == "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x99\x82");
}

TEST_CASE("maxCharacters of 0 is unlimited", "[text_input]")
{
  RawTextInput textInput;
  CHECK(textInput.maxCharacters() == 0);
  CHECK(textInput.maxLines() == 0);
  std::string longText(500, 'a');
  CHECK(textInput.insert(longText));
  CHECK(textInput.length() == 500);
}

TEST_CASE(
    "a rejected insert leaves selection and undo history alone",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.maxCharacters(3);
  CHECK(textInput.insert("abc"));
  textInput.cursor(Cursor(CursorPosition(1), CursorPosition(1)));
  CHECK_FALSE(textInput.insert('x'));
  CHECK(textInput.cursor().start().codePointIndex() == 1);
  CHECK(textInput.cursor().end().codePointIndex() == 1);

  // The rejected insert added no journal entry: one undo takes back the
  // "abc" insert.
  textInput.undo();
  CHECK(textInput.text() == "");
  textInput.redo();
  CHECK(textInput.text() == "abc");
}

TEST_CASE("text set from outside is kept even over the limits", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(10000.0f);
  textInput.maxCharacters(3);
  textInput.maxLines(1);

  textInput.text("abcdef\nghi");
  CHECK(textInput.text() == "abcdef\nghi");
  textInput.textPreserveCursor("abcdef\nghij");
  CHECK(textInput.text() == "abcdef\nghij");

  // Still over, so a selection replaced by something just as long is
  // refused...
  textInput.cursor(Cursor(CursorPosition(0), CursorPosition(1)));
  CHECK_FALSE(textInput.insert('x'));
  CHECK(textInput.text() == "abcdef\nghij");

  // ...while deleting always works.
  textInput.update(&factory);
  textInput.backspace(-1);
  CHECK(textInput.text() == "bcdef\nghij");
}

TEST_CASE("maxLines caps line breaks and truncates pastes", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(10000.0f);
  textInput.maxLines(2);
  CHECK(textInput.maxLines() == 2);

  // A trailing newline puts the cursor on a second (empty) line.
  CHECK(textInput.insert("one\n"));
  CHECK(textInput.text() == "one\n");
  CHECK_FALSE(textInput.insert("\n"));
  CHECK(textInput.text() == "one\n");

  // The paste stops before the line break that would start a third line.
  CHECK(textInput.insert("two\nthree\nfour"));
  CHECK(textInput.text() == "one\ntwo");
  textInput.update(&factory);
  CHECK(textInput.shape().lineCount() == 2);

  // More on the same line still goes in.
  CHECK(textInput.insert("!"));
  CHECK(textInput.text() == "one\ntwo!");
}

TEST_CASE("maxLines counts wrapped lines", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;

  float wordWidth;
  {
    RawTextInput probe;
    probe.font(font);
    probe.fontSize(32.0f);
    probe.insert("word");
    probe.update(&factory);
    wordWidth = probe.bounds().width();
  }
  REQUIRE(wordWidth > 0.0f);

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.sizing(TextSizing::autoHeight);
  // Room for one word per line, never two.
  textInput.maxWidth(wordWidth * 1.5f);

  // Without a limit the paste wraps onto four lines.
  std::string paste = "word word word word";
  CHECK(textInput.insert(paste));
  textInput.update(&factory);
  REQUIRE(textInput.shape().lineCount() == 4);

  textInput.text("");
  textInput.maxLines(2);
  CHECK(textInput.insert(paste));
  std::string kept = textInput.text();
  CHECK(kept.size() < paste.size());
  CHECK(kept.rfind("word word", 0) == 0);
  textInput.update(&factory);
  CHECK(textInput.shape().lineCount() <= 2);

  // A line break on the last allowed line is refused.
  textInput.cursor(Cursor::collapsed(CursorPosition(0)));
  CHECK_FALSE(textInput.insert("\n"));
  CHECK(textInput.text() == kept);
}

TEST_CASE(
    "maxLines wraps against the space the layout offers, not the hug",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;
  const float available = 900.0f;

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.sizing(TextSizing::autoHeight);
  textInput.hugsWidth(true);
  textInput.insert("123\n123\n123");

  // Lay out the way TextInput does: measure against the space available,
  // then wrap at the width the text actually took (it hugs its text).
  auto layout = [&]() {
    AABB measured = textInput.measure(
        available,
        std::numeric_limits<float>::max());
    textInput.maxWidth(measured.width());
    textInput.update(&factory);
  };
  layout();
  REQUIRE(textInput.shape().lineCount() == 3);
  textInput.maxLines(3);

  // Widening the widest line just grows the field, it doesn't wrap.
  textInput.cursor(Cursor::collapsed(CursorPosition(3)));
  CHECK(textInput.insert('4'));
  CHECK(textInput.text() == "1234\n123\n123");
  layout();
  CHECK(textInput.shape().lineCount() == 3);

  // Running past the space available does wrap, so it's held back.
  textInput.cursor(Cursor::collapsed(CursorPosition(4)));
  CHECK(textInput.insert(" and a much longer run of words that cannot "
                         "possibly fit in the space that is available"));
  layout();
  CHECK(textInput.shape().lineCount() == 3);
  CHECK(textInput.text().rfind("1234 and a", 0) == 0);
  CHECK(
      textInput.text().size() <
      std::string("1234 and a much longer run of words that cannot "
                  "possibly fit in the space that is available\n123\n123")
          .size());
}

TEST_CASE(
    "the cursor lands on an empty line, not the line after it",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(900.0f);
  textInput.insert("123\n\n123");
  textInput.update(&factory);
  REQUIRE(textInput.shape().lineCount() == 3);

  // The empty middle line is just the line break at index 4; index 5 is
  // already the start of the last line.
  auto middle = textInput.cursorVisualPosition(CursorPosition(1, 4));
  REQUIRE(middle.found());
  float middleY = (middle.top() + middle.bottom()) / 2.0f;
  for (float x : {0.0f, 60.0f})
  {
    textInput.moveCursorTo(Vec2D(x, middleY));
    CHECK(textInput.cursor().end().codePointIndex() == 4);
    CHECK(textInput.cursor().end().lineIndex() == 1);
  }

  textInput.cursor(Cursor::collapsed(CursorPosition(8)));
  textInput.update(&factory);
  textInput.cursorUp();
  CHECK(textInput.cursor().end().codePointIndex() == 4);

  textInput.cursor(Cursor::collapsed(CursorPosition(0)));
  textInput.update(&factory);
  textInput.cursorDown();
  CHECK(textInput.cursor().end().codePointIndex() == 4);

  CHECK(textInput.insert('x'));
  CHECK(textInput.text() == "123\nx\n123");
}

TEST_CASE(
    "Enter that leaves an empty line keeps the caret with the text",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;

  struct Case
  {
    const char* text;
    uint32_t enterAt;
    uint32_t lineAfter;
  };
  // Enter at the start of a line leaves an empty line behind the caret;
  // the caret must draw on the line it types into.
  for (Case c : {Case{"123", 0, 1}, Case{"123\n123", 4, 2}})
  {
    RawTextInput textInput;
    textInput.font(font);
    textInput.fontSize(32.0f);
    textInput.sizing(TextSizing::autoHeight);
    textInput.maxWidth(900.0f);
    textInput.insert(c.text);
    textInput.cursor(Cursor::collapsed(CursorPosition(c.enterAt)));
    textInput.update(&factory);

    CHECK(textInput.insert("\n"));
    textInput.update(&factory);
    CHECK(textInput.cursor().end().codePointIndex() == c.enterAt + 1);
    CHECK(textInput.cursor().end().lineIndex() == c.lineAfter);

    // Where it draws is where it types.
    auto drawn = textInput.cursorVisualPosition();
    auto line = textInput.cursorVisualPosition(
        CursorPosition(c.lineAfter, c.enterAt + 1));
    REQUIRE(drawn.found());
    CHECK(drawn.top() == line.top());

    // Home and End stay on the empty line rather than running into the
    // next one.
    textInput.cursor(Cursor::collapsed(CursorPosition(c.enterAt)));
    textInput.update(&factory);
    REQUIRE(textInput.cursor().end().lineIndex() == c.lineAfter - 1);
    textInput.cursorRight(CursorBoundary::line);
    CHECK(textInput.cursor().end().codePointIndex() == c.enterAt);
  }
}

TEST_CASE(
    "maxLines lets a full field's empty line be typed into",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.sizing(TextSizing::autoHeight);
  textInput.hugsWidth(true);
  textInput.insert("123\n\n123");
  AABB measured = textInput.measure(900.0f, std::numeric_limits<float>::max());
  textInput.maxWidth(measured.width());
  textInput.update(&factory);
  textInput.maxLines(3);

  textInput.cursorUp();
  CHECK(textInput.insert('x'));
  CHECK(textInput.text() == "123\nx\n123");
}

TEST_CASE(
    "maxLines keeps the longest paste that fits when wrapping isn't monotonic",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;
  // Wide enough for "aa cc", "cbbbb" and "xbbbb", too narrow for "ccbbbb" and
  // "xxbbbb", which then break mid-word onto a third line.
  const float width = 100.0f;
  auto lines = [&](const char* text) {
    RawTextInput probe;
    probe.font(font);
    probe.fontSize(32.0f);
    probe.sizing(TextSizing::autoHeight);
    probe.maxWidth(width);
    probe.insert(text);
    probe.update(&factory);
    return probe.shape().lineCount();
  };
  // Pasting "cc xx" before "bbbb": a prefix ending mid-word joins "bbbb", so
  // the line counts for prefixes 1 to 5 go 2, 3, 2, 2, 3.
  REQUIRE(lines("aa cbbbb") == 2);
  REQUIRE(lines("aa ccbbbb") == 3);
  REQUIRE(lines("aa cc bbbb") == 2);
  REQUIRE(lines("aa cc xbbbb") == 2);
  REQUIRE(lines("aa cc xxbbbb") == 3);

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(width);
  textInput.insert("aa bbbb");
  textInput.maxLines(2);
  textInput.cursor(Cursor::collapsed(CursorPosition(3)));
  CHECK(textInput.insert("cc xx"));
  CHECK(textInput.text() == "aa cc xbbbb");
}

TEST_CASE(
    "maxLines wraps at maxWidth unless the layout hugs the text",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;

  float wordWidth;
  {
    RawTextInput probe;
    probe.font(font);
    probe.fontSize(32.0f);
    probe.insert("word");
    probe.update(&factory);
    wordWidth = probe.bounds().width();
  }

  for (bool hugs : {false, true})
  {
    RawTextInput textInput;
    textInput.font(font);
    textInput.fontSize(32.0f);
    textInput.sizing(TextSizing::autoHeight);
    textInput.hugsWidth(hugs);
    // Measured with plenty of room, then narrowed to one word per line.
    textInput.measure(10000.0f, std::numeric_limits<float>::max());
    textInput.maxWidth(wordWidth * 1.5f);
    textInput.maxLines(2);

    CHECK(textInput.insert("word word word word"));
    if (hugs)
    {
      // A hugging field grows to the measured width, where it all fits.
      CHECK(textInput.text() == "word word word word");
    }
    else
    {
      // A field held at maxWidth keeps what fits on its two lines there.
      textInput.update(&factory);
      CHECK(textInput.shape().lineCount() <= 2);
      CHECK(textInput.text().rfind("word word", 0) == 0);
      CHECK(textInput.text().rfind("word word word") == std::string::npos);
    }
  }
}

TEST_CASE(
    "maxLines refuses a large paste into a full field quickly",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(900.0f);
  // 999 line breaks make 1000 lines: full.
  const std::string full(999, '\n');
  textInput.insert(full);
  textInput.maxLines(1000);
  textInput.cursor(Cursor::collapsed(CursorPosition(0)));
  textInput.limitShapeCount = 0;

  CHECK_FALSE(textInput.insert(std::string(1000, '\n')));
  CHECK(textInput.text() == full);
  // The paragraphs after the insertion point stay in the bound, so the
  // search never counts down through the paste.
  CHECK(textInput.limitShapeCount < 20);
}

TEST_CASE(
    "maxLines keeps a large paste into a long paragraph within the limit",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  NoOpFactory factory;

  std::string words;
  for (int i = 0; i < 200; i++)
  {
    words += "word ";
  }

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(300.0f);
  textInput.insert(words);
  textInput.update(&factory);
  // One more line than the paragraph takes, then paste at its start: the
  // rest of the paragraph spans far more than the count down's budget.
  uint32_t lines = textInput.shape().lineCount();
  textInput.maxLines(lines + 1);
  textInput.cursor(Cursor::collapsed(CursorPosition(0)));
  textInput.limitShapeCount = 0;

  CHECK(textInput.insert(words));
  textInput.update(&factory);
  CHECK(textInput.shape().lineCount() <= lines + 1);
  CHECK(textInput.text().size() > words.size());
  CHECK(textInput.limitShapeCount < 100);
}

TEST_CASE("an empty right to left line holds its own break", "[text_input]")
{
  auto font = loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf");
  NoOpFactory factory;

  // Two line separators inside one right to left run: the middle line is
  // empty and its break is the separator at index 2. Its iterator begins at
  // the glyph before, the separator ending the line above.
  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(900.0f);
  textInput.insert("\xD8\xA8\xE2\x80\xA8\xE2\x80\xA8\xD8\xA8");
  textInput.update(&factory);
  const auto& shape = textInput.shape();
  REQUIRE(shape.lineCount() == 3);
  const OrderedLine& middle = shape.orderedLines()[1];
  REQUIRE(middle.isEmpty());
  REQUIRE(middle.lastRun()->dir() == TextDirection::rtl);

  CHECK(middle.firstCodePointIndex(shape.glyphLookup()) == 2);
  CHECK(middle.lastCodePointIndex(shape.glyphLookup()) == 2);
  for (float x : {0.0f, 60.0f})
  {
    CHECK(CursorPosition::fromLineX(1, x, shape).codePointIndex() == 2);
  }

  // Its line boundaries are where the cursor already is.
  textInput.cursor(Cursor::collapsed(CursorPosition(1, 2)));
  textInput.cursorLeft(CursorBoundary::line);
  CHECK(textInput.cursor().end().codePointIndex() == 2);
  textInput.cursorRight(CursorBoundary::line);
  CHECK(textInput.cursor().end().codePointIndex() == 2);
}

TEST_CASE(
    "maxLines counts the lines an obscured input displays",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(10000.0f);
  textInput.maxLines(1);

  CHECK(textInput.insert("a\nb"));
  CHECK(textInput.text() == "a");

  // Obscured, the line break displays as a bullet on the same line.
  textInput.text("");
  textInput.obscured(true);
  CHECK(textInput.insert("a\nb"));
  CHECK(textInput.text() == "a\nb");
}

TEST_CASE("the maxLines check leaves measure() correct", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

  RawTextInput textInput;
  textInput.font(font);
  textInput.fontSize(32.0f);
  textInput.sizing(TextSizing::autoHeight);
  textInput.maxWidth(10000.0f);
  textInput.insert("hello");
  AABB before = textInput.measure(10000.0f, 10000.0f);

  // A rejected insert shapes its candidate in the measuring shape.
  textInput.maxLines(1);
  CHECK_FALSE(textInput.insert("\nsecond line that is much wider"));
  AABB after = textInput.measure(10000.0f, 10000.0f);
  CHECK(after.width() == before.width());
  CHECK(after.height() == before.height());
}

static void checkSameShape(const FullyShapedText& a, const FullyShapedText& b)
{
  REQUIRE(a.paragraphs().size() == b.paragraphs().size());
  for (size_t p = 0; p < a.paragraphs().size(); p++)
  {
    const Paragraph& pa = a.paragraphs()[p];
    const Paragraph& pb = b.paragraphs()[p];
    CHECK(pa.level == pb.level);
    REQUIRE(pa.runs.size() == pb.runs.size());
    for (size_t r = 0; r < pa.runs.size(); r++)
    {
      const GlyphRun& ra = pa.runs[r];
      const GlyphRun& rb = pb.runs[r];
      CHECK(ra.font == rb.font);
      CHECK(ra.level == rb.level);
      REQUIRE(ra.glyphs.size() == rb.glyphs.size());
      for (size_t g = 0; g < ra.glyphs.size(); g++)
      {
        CHECK(ra.glyphs[g] == rb.glyphs[g]);
        CHECK(ra.textIndices[g] == rb.textIndices[g]);
        CHECK(ra.advances[g] == rb.advances[g]);
        CHECK(ra.offsets[g].x == rb.offsets[g].x);
        CHECK(ra.offsets[g].y == rb.offsets[g].y);
      }
      REQUIRE(ra.xpos.size() == rb.xpos.size());
      for (size_t g = 0; g < ra.xpos.size(); g++)
      {
        CHECK(ra.xpos[g] == rb.xpos[g]);
      }
      REQUIRE(ra.breaks.size() == rb.breaks.size());
      for (size_t i = 0; i < ra.breaks.size(); i++)
      {
        CHECK(ra.breaks[i] == rb.breaks[i]);
      }
      REQUIRE(ra.joiners.size() == rb.joiners.size());
      for (size_t i = 0; i < ra.joiners.size(); i++)
      {
        CHECK(ra.joiners[i] == rb.joiners[i]);
      }
    }
  }
  REQUIRE(a.glyphLookup().size() == b.glyphLookup().size());
  for (uint32_t i = 0; i < (uint32_t)a.glyphLookup().size(); i++)
  {
    CHECK(a.glyphLookup()[i] == b.glyphLookup()[i]);
  }
  REQUIRE(a.paragraphLines().size() == b.paragraphLines().size());
  for (size_t p = 0; p < a.paragraphLines().size(); p++)
  {
    REQUIRE(a.paragraphLines()[p].size() == b.paragraphLines()[p].size());
    for (size_t l = 0; l < a.paragraphLines()[p].size(); l++)
    {
      const GlyphLine& la = a.paragraphLines()[p][l];
      const GlyphLine& lb = b.paragraphLines()[p][l];
      CHECK(la.startRunIndex == lb.startRunIndex);
      CHECK(la.startGlyphIndex == lb.startGlyphIndex);
      CHECK(la.endRunIndex == lb.endRunIndex);
      CHECK(la.endGlyphIndex == lb.endGlyphIndex);
      CHECK(la.startX == lb.startX);
      CHECK(la.top == lb.top);
      CHECK(la.baseline == lb.baseline);
      CHECK(la.bottom == lb.bottom);
    }
  }
  REQUIRE(a.orderedLines().size() == b.orderedLines().size());
  for (size_t l = 0; l < a.orderedLines().size(); l++)
  {
    CHECK(a.orderedLines()[l].y() == b.orderedLines()[l].y());
  }
  CHECK_AABB(a.bounds(), b.bounds());
}

static void configureEditingInput(RawTextInput& input, rcp<Font> font)
{
  input.font(font);
  input.fontSize(24.0f);
  input.sizing(TextSizing::autoHeight);
  input.maxWidth(300.0f);
}

static void checkMatchesFullShape(
    RawTextInput& input,
    rcp<Font> font,
    Factory* factory,
    const char* step)
{
  INFO(step);
  input.update(factory);
  CHECK(input.fullShapeCount == 1);
  RawTextInput fresh;
  configureEditingInput(fresh, font);
  fresh.text(input.text());
  fresh.update(factory);
  checkSameShape(input.shape(), fresh.shape());
}

static void placeCursor(RawTextInput& input, uint32_t index)
{
  input.cursor(Cursor::collapsed(CursorPosition(index)));
}

static uint32_t paragraphStart(const RawTextInput& input, int paragraph)
{
  std::string text = input.text();
  uint32_t codePoints = 0;
  int seen = 0;
  const uint8_t* ptr = (const uint8_t*)text.data();
  const uint8_t* end = ptr + text.size();
  while (ptr < end && seen < paragraph)
  {
    Unichar c = UTF::NextUTF8(&ptr, end);
    codePoints++;
    if (c == '\n')
    {
      seen++;
    }
  }
  return codePoints;
}

TEST_CASE("editing reshapes only the edited paragraphs", "[text_input]")
{
  auto font = loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf");
  REQUIRE(font != nullptr);
  NoOpFactory factory;

  std::string text;
  const char* lines[] = {
      "plain left to right words that wrap past the edge of the field",
      "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 "
      "\xd8\xa8\xd8\xa7\xd9\x84\xd8\xb9\xd8\xa7\xd9\x84\xd9\x85",
      "mixed \xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 direction line",
      "combining e\xcc\x81 marks a\xcc\x88 here",
      "",
      "short",
  };
  for (int i = 0; i < 24; i++)
  {
    text += lines[i % 6];
    text += i % 5 == 4 ? "\r\n" : "\n";
  }
  text += "last paragraph without a separator";

  RawTextInput input;
  configureEditingInput(input, font);
  input.text(text);
  input.update(&factory);
  CHECK(input.fullShapeCount == 1);

  placeCursor(input, paragraphStart(input, 6) + 3);
  input.insert("x");
  checkMatchesFullShape(input, font, &factory, "type in a paragraph");

  placeCursor(input, paragraphStart(input, 7) + 2);
  input.insert("\n");
  checkMatchesFullShape(input, font, &factory, "type a newline");

  input.backspace(-1);
  checkMatchesFullShape(input, font, &factory, "delete the newline");

  placeCursor(input, paragraphStart(input, 9) - 1);
  input.backspace(1);
  checkMatchesFullShape(
      input,
      font,
      &factory,
      "delete the separator ending a paragraph");

  input.cursor(Cursor(
      CursorPosition(paragraphStart(input, 3) + 4),
      CursorPosition(paragraphStart(input, 6) + 2)));
  input.erase();
  checkMatchesFullShape(input, font, &factory, "delete across paragraphs");

  placeCursor(input, paragraphStart(input, 10) + 1);
  input.insert("one\ntwo\r\nthree \xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7");
  checkMatchesFullShape(input, font, &factory, "paste several lines");

  placeCursor(input, 0);
  input.insert("first ");
  checkMatchesFullShape(input, font, &factory, "edit the first paragraph");

  input.moveCursorToEnd();
  input.insert(" end");
  checkMatchesFullShape(input, font, &factory, "edit the last paragraph");

  // A CR ends its own paragraph until an LF typed after it joins the two.
  placeCursor(input, paragraphStart(input, 14));
  input.insert("\r");
  checkMatchesFullShape(input, font, &factory, "type a CR");
  placeCursor(input, paragraphStart(input, 14) + 1);
  input.insert("\n");
  checkMatchesFullShape(input, font, &factory, "join a CR and an LF");

  placeCursor(input, paragraphStart(input, 9));
  input.insert("a");
  placeCursor(input, paragraphStart(input, 7) + 2);
  input.insert("b");
  checkMatchesFullShape(input, font, &factory, "two edits before one update");

  input.undo();
  checkMatchesFullShape(input, font, &factory, "undo");
  input.redo();
  checkMatchesFullShape(input, font, &factory, "redo");

  std::string changed = input.text();
  changed.insert(changed.find('\n', changed.size() / 2) + 1, "outside");
  input.textPreserveCursor(changed);
  checkMatchesFullShape(input, font, &factory, "text set from outside");

  CHECK(input.fullShapeCount == 1);
  CHECK(input.reshapeCount >= 13);

  input.textPreserveCursor(input.text());
  uint32_t before = input.reshapeCount + input.fullShapeCount;
  input.update(&factory);
  CHECK(input.reshapeCount + input.fullShapeCount == before);
}

static std::string linesOfText(int count)
{
  std::string text;
  for (int i = 0; i < count; i++)
  {
    text += "a line of the file\n";
  }
  return text;
}

TEST_CASE(
    "the undo history holds edits, not copies of the text",
    "[text_input]")
{
  std::string text = linesOfText(2000);

  RawTextInput input;
  input.font(loadFont("assets/fonts/Inter_18pt-Regular.ttf"));
  input.text(text);
  CHECK(input.journalCodePoints() == 0);

  placeCursor(input, 100);
  input.insert("x");
  CHECK(input.journalCodePoints() == 1);

  // The removed code point is kept for undo.
  input.backspace(-1);
  CHECK(input.journalCodePoints() == 2);
  CHECK(input.text() == text);

  // Replacing a selection keeps what it removed and what it added.
  input.cursor(Cursor(CursorPosition(10), CursorPosition(14)));
  input.insert("yz");
  CHECK(input.journalCodePoints() == 8);

  // Text set from outside only counts where it differs.
  std::string changed = input.text();
  changed.insert(20000, "new");
  input.textPreserveCursor(changed);
  CHECK(input.journalCodePoints() == 11);

  input.undo();
  input.undo();
  input.undo();
  input.undo();
  CHECK(input.text() == text);
  input.redo();
  input.redo();
  input.redo();
  input.redo();
  CHECK(input.text() == changed);
  CHECK(input.journalCodePoints() == 11);
}

TEST_CASE("undo takes back an erase made without an entry", "[text_input]")
{
  RawTextInput input;
  input.text("one two three");
  input.moveCursorToEnd();
  input.insert(" four");

  input.cursor(Cursor(CursorPosition(0), CursorPosition(4)));
  input.erase();
  CHECK(input.text() == "two three four");

  // The erase has no entry of its own, so it goes with the one before it.
  input.undo();
  CHECK(input.text() == "one two three");
  input.redo();
  CHECK(input.text() == "one two three four");

  input.cursor(Cursor(CursorPosition(0), CursorPosition(4)));
  input.erase();
  input.moveCursorToEnd();
  input.insert("!");
  CHECK(input.text() == "two three four!");
  input.undo();
  CHECK(input.text() == "one two three four");
  input.redo();
  CHECK(input.text() == "two three four!");

  // Text set from outside starts over, dropping an erase no entry holds.
  input.cursor(Cursor(CursorPosition(0), CursorPosition(4)));
  input.erase();
  input.text("five six");
  input.moveCursorToEnd();
  input.insert(" seven");
  input.undo();
  CHECK(input.text() == "five six");
  input.undo();
  CHECK(input.text() == "five six");
}

TEST_CASE(
    "an edit that changes nothing still resolves the cursor",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  NoOpFactory factory;
  RawTextInput input;
  configureEditingInput(input, font);
  input.text("one two three");
  input.update(&factory);

  placeCursor(input, 4);
  input.update(&factory);
  input.insert(std::string());
  input.update(&factory);
  CHECK(input.cursor().start().hasLineIndex());
  CHECK(input.text() == "one two three");

  // A selection can span two lines without holding a code point.
  input.cursor(Cursor(CursorPosition(0, 4), CursorPosition(1, 4)));
  input.update(&factory);
  input.backspace(-1);
  input.update(&factory);
  CHECK(input.cursor().start().hasLineIndex());
  CHECK(input.text() == "one two three");
}

TEST_CASE(
    "measuring at the laid out width breaks no lines again",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  NoOpFactory factory;

  std::string text;
  for (int i = 0; i < 40; i++)
  {
    text += "a paragraph long enough to wrap past the edge of the field\n";
  }
  RawTextInput input;
  configureEditingInput(input, font);
  input.text(text);
  input.update(&factory);
  uint64_t paragraphs = input.shape().paragraphs().size();

  const float noLimit = std::numeric_limits<float>::max();
  uint64_t before = GlyphLine::sm_breakCount;
  AABB measured = input.measure(300.0f, noLimit);
  CHECK(GlyphLine::sm_breakCount == before);
  CHECK(measured.width() == input.bounds().width());
  CHECK(measured.height() == input.bounds().height());

  // Any other width has to break them, without touching the layout.
  AABB narrow = input.measure(200.0f, noLimit);
  CHECK(GlyphLine::sm_breakCount == before + paragraphs);
  CHECK(narrow.height() > measured.height());
  CHECK(input.bounds().height() == measured.height());
}

TEST_CASE("an edit breaks only the lines of its paragraphs", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  NoOpFactory factory;

  std::string text;
  for (int i = 0; i < 200; i++)
  {
    text += "a paragraph long enough to wrap past the edge of the field\n";
  }
  RawTextInput input;
  configureEditingInput(input, font);
  input.text(text);
  input.update(&factory);
  uint64_t paragraphs = input.shape().paragraphs().size();

  const float noLimit = std::numeric_limits<float>::max();
  placeCursor(input, paragraphStart(input, 100) + 5);
  uint64_t before = GlyphLine::sm_breakCount;
  input.insert("x");
  input.measure(300.0f, noLimit);
  input.update(&factory);
  CHECK(GlyphLine::sm_breakCount == before + 1);

  // A new line splits the paragraph in two.
  before = GlyphLine::sm_breakCount;
  input.insert("\n");
  input.measure(300.0f, noLimit);
  input.update(&factory);
  CHECK(GlyphLine::sm_breakCount == before + 2);
  CHECK(input.shape().paragraphs().size() == paragraphs + 1);

  // The first paragraph's first line is spaced on its own.
  placeCursor(input, 0);
  input.insert("x");
  checkMatchesFullShape(input, font, &factory, "edit the first paragraph");
  input.cursor(
      Cursor(CursorPosition(0), CursorPosition(paragraphStart(input, 2))));
  input.erase();
  checkMatchesFullShape(input, font, &factory, "delete the first paragraphs");

  // A new width breaks them all.
  before = GlyphLine::sm_breakCount;
  input.maxWidth(250.0f);
  input.update(&factory);
  CHECK(GlyphLine::sm_breakCount == before + paragraphs - 1);
}

TEST_CASE(
    "text paths hold only the lines near the visible range",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  NoOpFactory factory;

  std::string text = linesOfText(400);
  RawTextInput input;
  configureEditingInput(input, font);
  input.text(text);
  input.update(&factory);
  size_t every = input.textPath()->numContours();
  float height = input.bounds().height();
  float lineHeight = height / 401.0f;
  REQUIRE(every > 400);

  // Ten lines show, and as many again are built past each edge.
  float top = lineHeight * 200.0f;
  float bottom = top + lineHeight * 10.0f;
  CHECK(input.visibleRange(top, bottom, &factory));
  size_t culled = input.textPath()->numContours();
  CHECK(culled > 0);
  CHECK(culled < every / 10);
  AABB built = input.textPath()->rawPath()->bounds();
  CHECK(built.minY <= top);
  CHECK(built.maxY >= bottom);
  CHECK(built.minY > top - lineHeight * 12.0f);
  CHECK(built.maxY < bottom + lineHeight * 12.0f);

  // A scroll within what was built keeps the paths.
  CHECK(!input.visibleRange(
      top + lineHeight * 5.0f,
      bottom + lineHeight * 5.0f,
      &factory));
  CHECK(input.textPath()->numContours() == culled);

  // One past it builds them around the new range.
  top += lineHeight * 30.0f;
  bottom += lineHeight * 30.0f;
  CHECK(input.visibleRange(top, bottom, &factory));
  built = input.textPath()->rawPath()->bounds();
  CHECK(built.minY <= top);
  CHECK(built.maxY >= bottom);

  // An edit builds for the same range.
  placeCursor(input, 19 * 230 + 4);
  input.insert("x");
  input.update(&factory);
  CHECK(input.textPath()->numContours() < every / 10);
  built = input.textPath()->rawPath()->bounds();
  CHECK(built.minY <= top);
  CHECK(built.maxY >= bottom);

  // min > max goes back to every line.
  CHECK(input.visibleRange(1.0f, 0.0f, &factory));
  CHECK(input.textPath()->numContours() > every);
  CHECK(!input.visibleRange(1.0f, 0.0f, &factory));
}

TEST_CASE(
    "an edit or a scroll builds only the lines it brings in",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  NoOpFactory factory;

  // Every third paragraph wraps onto more lines.
  std::string text;
  for (int i = 0; i < 300; i++)
  {
    text += i % 3 == 0
                ? "a paragraph long enough to wrap past the edge of the field\n"
                : "a line of the file\n";
  }
  RawTextInput input;
  configureEditingInput(input, font);
  input.text(text);
  input.update(&factory);
  float lineHeight = input.shape().orderedLines()[1].y() -
                     input.shape().orderedLines()[0].y();
  auto firstLineAt = [&](float y) {
    const auto& lines = input.shape().orderedLines();
    for (uint32_t i = 0; i < (uint32_t)lines.size(); i++)
    {
      if (lines[i].bottom() >= y)
      {
        return lines[i].firstCodePointIndex(input.shape().glyphLookup());
      }
    }
    return (uint32_t)0;
  };
  auto topOfLineAt = [&](uint32_t codePointIndex) {
    for (const OrderedLine& line : input.shape().orderedLines())
    {
      if (line.firstCodePointIndex(input.shape().glyphLookup()) ==
          codePointIndex)
      {
        return line.top();
      }
    }
    return 0.0f;
  };

  // Ten lines show, the fourth of them a paragraph of one line.
  uint32_t oneLine = paragraphStart(input, 151);
  float top = topOfLineAt(oneLine) - lineHeight * 3.0f;
  float bottom = top + lineHeight * 10.0f;
  REQUIRE(top > lineHeight * 100.0f);
  REQUIRE(input.visibleRange(top, bottom, &factory));

  // The same text and range built in one go, to compare against.
  auto checkMatchesFreshPaths = [&](const char* step) {
    INFO(step);
    RawTextInput fresh;
    fresh.font(font);
    fresh.fontSize(input.fontSize());
    fresh.sizing(TextSizing::autoHeight);
    fresh.maxWidth(input.maxWidth());
    fresh.text(input.text());
    fresh.visibleRange(top, bottom, &factory);
    fresh.update(&factory);
    REQUIRE(!fresh.textPath()->rawPath()->empty());
    CHECK(*input.textPath()->rawPath() == *fresh.textPath()->rawPath());
  };

  uint32_t before = input.lineBuildCount;
  placeCursor(input, oneLine + 2);
  input.insert("x");
  input.update(&factory);
  CHECK(input.lineBuildCount == before + 1);
  checkMatchesFreshPaths("type in a line");

  // An edit below what is built builds nothing.
  before = input.lineBuildCount;
  placeCursor(input, paragraphStart(input, 200) + 2);
  input.insert("x");
  input.update(&factory);
  CHECK(input.lineBuildCount == before);
  checkMatchesFreshPaths("type below");

  // Scrolling past what was built keeps the lines still in range.
  before = input.lineBuildCount;
  top += lineHeight * 12.0f;
  bottom += lineHeight * 12.0f;
  REQUIRE(input.visibleRange(top, bottom, &factory));
  CHECK(input.lineBuildCount - before > 10);
  CHECK(input.lineBuildCount - before < 16);
  checkMatchesFreshPaths("scroll down");

  before = input.lineBuildCount;
  top -= lineHeight * 24.0f;
  bottom -= lineHeight * 24.0f;
  REQUIRE(input.visibleRange(top, bottom, &factory));
  CHECK(input.lineBuildCount - before > 10);
  CHECK(input.lineBuildCount - before < 28);
  checkMatchesFreshPaths("scroll up");

  // A new line moves the built lines below it, which are built again where
  // they now sit, and leaves the ones above alone.
  before = input.lineBuildCount;
  placeCursor(input, firstLineAt(top + lineHeight * 5.0f) + 2);
  input.insert("\n");
  input.update(&factory);
  CHECK(input.lineBuildCount - before > 1);
  CHECK(input.lineBuildCount - before < 28);
  checkMatchesFreshPaths("type a newline");

  // Enough to wrap a built paragraph onto another line.
  placeCursor(input, firstLineAt(top + lineHeight * 2.0f) + 2);
  input.insert(" words that push the rest of this paragraph down a line ");
  input.update(&factory);
  checkMatchesFreshPaths("wrap a paragraph");

  // An edit above what is built moves every line built.
  placeCursor(input, 4);
  input.insert("\n");
  input.update(&factory);
  checkMatchesFreshPaths("type a newline above");

  input.undo();
  input.update(&factory);
  checkMatchesFreshPaths("undo");

  // A paragraph that starts above what is built and ends inside it.
  uint32_t straddling = firstLineAt(top - lineHeight * 10.0f);
  input.cursor(
      Cursor(CursorPosition(straddling - 30), CursorPosition(straddling + 30)));
  input.erase();
  input.update(&factory);
  checkMatchesFreshPaths("delete across the first built line");

  input.cursor(Cursor(
      CursorPosition(firstLineAt(top - lineHeight * 20.0f)),
      CursorPosition(firstLineAt(bottom + lineHeight * 20.0f))));
  input.erase();
  input.update(&factory);
  checkMatchesFreshPaths("delete every built line");

  // Every line, then a range again.
  float rangeTop = top;
  float rangeBottom = bottom;
  top = 1.0f;
  bottom = 0.0f;
  REQUIRE(input.visibleRange(top, bottom, &factory));
  placeCursor(input, 10);
  input.insert("y");
  input.update(&factory);
  checkMatchesFreshPaths("every line");
  top = rangeTop;
  bottom = rangeBottom;
  REQUIRE(input.visibleRange(top, bottom, &factory));
  checkMatchesFreshPaths("a range again");

  // A new width lays every line out again.
  input.maxWidth(250.0f);
  input.update(&factory);
  checkMatchesFreshPaths("a new width");
  input.fontSize(20.0f);
  input.update(&factory);
  checkMatchesFreshPaths("a new font size");
}

TEST_CASE("a cursor resolves to the first line that holds it", "[text_input]")
{
  auto font = loadFont("assets/fonts/IBMPlexSansArabic-Regular.ttf");
  REQUIRE(font != nullptr);
  NoOpFactory factory;

  // Line separators, a line starting where a run ends, wrapping words, right
  // to left runs and trailing spaces.
  const char* texts[] = {
      "e\xe2\x80\xa8 ",
      "-nx",
      // An empty line that ends where a right to left run starts.
      "ab\xd9\x85\xd8\xb1",
      "\xd7\x91\xd7\x92"
      "23\xd7\x91\xd7\x92  \xe2\x80\x8d",
      // A line that starts where a run ends.
      "(\xe2\x80\xa8"
      "abc",
      "one two three four five six seven eight nine ten\n\n eleven\n",
      "mixed \xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 direction "
      "\xd8\xa8\xd8\xa7\xd9\x84\xd8\xb9\xd8\xa7\xd9\x84\xd9\x85 line\x0b end ",
  };
  for (const char* text : texts)
  {
    for (float width : {0.0f, 5.0f, 12.0f, 60.0f, 300.0f})
    {
      RawTextInput input;
      configureEditingInput(input, font);
      input.maxWidth(width);
      input.text(text);
      input.update(&factory);
      const auto& lines = input.shape().orderedLines();
      for (uint32_t index = 0; index <= (uint32_t)input.length(); index++)
      {
        uint32_t first = 0;
        while (first < lines.size() && !lines[first].containsCodePointIndex(
                                           input.shape().glyphLookup(),
                                           index))
        {
          first++;
        }
        CursorPosition position(index);
        position.resolveLine(input.shape());
        INFO(text << " at width " << width << " index " << index);
        CHECK(position.lineIndex() == first);
        CHECK(
            CursorPosition::atIndex(index, input.shape()).lineIndex() <
            lines.size());
      }
    }
  }
}

TEST_CASE(
    "an edit beside an information separator shapes like a full shape",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  NoOpFactory factory;

  // The separator ends a paragraph, but the hyphen after it still breaks by
  // what came before.
  std::string text;
  for (int i = 0; i < 8; i++)
  {
    text += "a paragraph of the file\n";
  }
  text += "c\x1c-e x\n";
  for (int i = 0; i < 8; i++)
  {
    text += "a paragraph of the file\n";
  }
  RawTextInput input;
  configureEditingInput(input, font);
  input.maxWidth(12.0f);
  input.text(text);
  input.update(&factory);

  auto checkMatchesFresh = [&](const char* step) {
    INFO(step);
    input.update(&factory);
    RawTextInput fresh;
    configureEditingInput(fresh, font);
    fresh.maxWidth(12.0f);
    fresh.text(input.text());
    fresh.update(&factory);
    checkSameShape(input.shape(), fresh.shape());
  };

  uint32_t separator = (uint32_t)text.find('\x1c');
  placeCursor(input, separator + 3);
  input.insert("y");
  checkMatchesFresh("type after the separator");
  placeCursor(input, separator);
  input.insert("-");
  checkMatchesFresh("type before the separator");
}

TEST_CASE(
    "erasing a word selected at the end keeps the text whole",
    "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  NoOpFactory factory;
  RawTextInput input;
  configureEditingInput(input, font);
  input.update(&factory);

  // In an empty input the word is the sentinel that ends the text.
  input.selectWord();
  input.backspace(-1);
  input.undo();
  input.redo();
  input.insert("a");
  CHECK(input.text() == "a");
  CHECK(input.length() == 1);

  input.text("ab ");
  input.moveCursorToEnd();
  input.selectWord();
  input.backspace(-1);
  CHECK(input.text() == "ab");
  input.insert("c");
  CHECK(input.text() == "abc");
  input.undo();
  input.undo();
  CHECK(input.text() == "ab ");
  input.redo();
  input.redo();
  CHECK(input.text() == "abc");
  input.update(&factory);
  CHECK(input.shape().paragraphs().size() == 1);
}

TEST_CASE("word joiners never stall the line breaker", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  NoOpFactory factory;

  // A joiner holds its neighbours together, so stepping back to find where a
  // line fits can run past where the line starts.
  const char* texts[] = {
      "\xe2\x81\xa0"
      "a\xe2\x81\xa0"
      "c",
      "ab\xe2\x81\xa0"
      "cd e\xe2\x81\xa0\xe2\x81\xa0"
      "f gh",
      "one \xe2\x81\xa0two\xe2\x81\xa0 three",
      // Joiners that end a word, where a step over them reaches the line
      // break after it.
      "a\xe2\x81\xa0"
      "b\xe2\x81\xa0"
      "c\xe2\x81\xa0\xe2\x81\xa0\nd e",
      "abc\xe2\x81\xa0\xe2\x81\xa0\n",
      // Joiners on both sides of a space, where the step reaches into the next
      // word.
      "a\xe2\x81\xa0"
      "b\xe2\x81\xa0"
      "c\xe2\x81\xa0 \xe2\x81\xa0"
      "defghi\xe2\x81\xa0x",
  };
  for (const char* text : texts)
  {
    for (float width : {0.0f, 5.0f, 12.0f, 20.0f, 25.0f, 40.0f})
    {
      INFO(text << " at width " << width);
      RawTextInput input;
      configureEditingInput(input, font);
      input.maxWidth(width);
      input.text(text);
      input.update(&factory);

      size_t lineCount = 0;
      for (const auto& lines : input.shape().paragraphLines())
      {
        const GlyphLine* previous = nullptr;
        for (const GlyphLine& line : lines)
        {
          lineCount++;
          bool endsBeforeItStarts = line.endRunIndex < line.startRunIndex ||
                                    (line.endRunIndex == line.startRunIndex &&
                                     line.endGlyphIndex < line.startGlyphIndex);
          CHECK(!endsBeforeItStarts);
          bool startsInsidePrevious = previous != nullptr &&
                                      (line.startRunIndex <
                                           previous->endRunIndex ||
                                       (line.startRunIndex ==
                                            previous->endRunIndex &&
                                        line.startGlyphIndex <
                                            previous->endGlyphIndex));
          CHECK(!startsInsidePrevious);
          previous = &line;
        }
      }
      CHECK(lineCount != 0);
    }
  }

  // A line break the joiners carried a line past adds no line of its own.
  RawTextInput input;
  configureEditingInput(input, font);
  input.maxWidth(5.0f);
  input.text("abc\xe2\x81\xa0\xe2\x81\xa0\xe2\x80\xa8");
  input.update(&factory);
  CHECK(input.shape().lineCount() == 4);
}

TEST_CASE("a cursor past the end of the text edits at its end", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  NoOpFactory factory;
  RawTextInput input;
  configureEditingInput(input, font);
  input.text("foo ");
  input.update(&factory);

  // A word selected at the end stops short of the sentinel that ends the text.
  input.moveCursorToEnd();
  input.selectWord();
  CHECK(input.cursor().start().codePointIndex() == 3);
  CHECK(input.cursor().end().codePointIndex() == 4);
  input.clearSelection();
  input.backspace(-1);
  CHECK(input.text() == "foo");
  input.insert(" ");
  CHECK(input.text() == "foo ");

  // A cursor set from outside can still be past it.
  input.cursor(Cursor::collapsed(CursorPosition(9)));
  input.insert("x");
  CHECK(input.text() == "foo x");
  CHECK(input.length() == 5);
  input.cursor(Cursor::collapsed(CursorPosition(9)));
  input.backspace(-1);
  CHECK(input.text() == "foo ");
  input.cursor(Cursor::collapsed(CursorPosition(5)));
  input.backspace(1);
  CHECK(input.text() == "foo ");
  input.cursor(Cursor(CursorPosition(2), CursorPosition(9)));
  input.backspace(-1);
  CHECK(input.text() == "fo");
  CHECK(input.length() == 2);

  input.update(&factory);
  CHECK(input.shape().paragraphs().size() == 1);
  CHECK(input.cursor().start().hasLineIndex());
}

TEST_CASE(
    "backspace keeps the sentinel a font has no glyph for",
    "[text_input]")
{
  // Without a space glyph to stand in for it, the shaper drops the sentinel
  // and its code point joins the cluster before it.
  auto font = loadFont("assets/TwemojiMozilla.subset.ttf");
  REQUIRE(font != nullptr);
  NoOpFactory factory;
  RawTextInput input;
  configureEditingInput(input, font);
  input.insert("12");
  input.update(&factory);
  REQUIRE(input.shape().glyphLookup().count(1) == 2);

  input.backspace(-1);
  CHECK(input.text() == "1");
  CHECK(input.length() == 1);
  input.insert("3");
  CHECK(input.text() == "13");

  placeCursor(input, 1);
  input.backspace(1);
  CHECK(input.text() == "1");
  CHECK(input.length() == 1);
}

TEST_CASE("a word jump from the end of the text stays there", "[text_input]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  NoOpFactory factory;
  RawTextInput input;
  configureEditingInput(input, font);
  input.text("hello world");
  input.update(&factory);

  input.moveCursorToEnd();
  input.cursorRight(CursorBoundary::word);
  CHECK(input.cursor().end().codePointIndex() == 11);
  input.cursorRight(CursorBoundary::subWord);
  CHECK(input.cursor().end().codePointIndex() == 11);

  // Only white space lies between the cursor and the end.
  input.text("hello  ");
  placeCursor(input, 5);
  input.cursorRight(CursorBoundary::word);
  CHECK(input.cursor().end().codePointIndex() == 7);
}

TEST_CASE("a text input edits before its font arrives", "[text_input]")
{
  RawTextInput input;
  input.insert("ab cd");
  input.cursorLeft();
  CHECK(input.cursor().end().codePointIndex() == 4);
  input.cursorRight();
  input.cursorRight();
  CHECK(input.cursor().end().codePointIndex() == 5);
  input.backspace(-1);
  CHECK(input.text() == "ab c");
  input.cursor(Cursor::collapsed(CursorPosition(1)));
  input.backspace(1);
  CHECK(input.text() == "a c");
  input.cursorRight();
  input.cursorLeft(CursorBoundary::word);
  input.cursorUp();
  input.cursorDown();
  input.selectLine();
  input.selectWord();
  input.undo();
  CHECK(input.text() == "ab c");
}
#endif
