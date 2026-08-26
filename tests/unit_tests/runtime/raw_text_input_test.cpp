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

TEST_CASE("cursor's visual position computes correctly", "[text_input]")
{
    auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

    RawTextInput textInput;
    std::string defaultText =
        "this is some\nmultiline text input\nwith one final line\n";
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

    CHECK(textInput.cursor().start().codePointIndex() ==
          secondLine.firstCodePointIndex(textInput.shape().glyphLookup()));

    // Click to the right of the whole line of text.
    textInput.moveCursorTo(Vec2D(maxWidth + 20.0f, secondLine.y()));
    CHECK(textInput.cursor().start().codePointIndex() ==
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

    CHECK(textInput.cursor().start().codePointIndex() ==
          secondLine.firstCodePointIndex(textInput.shape().glyphLookup()));

    // Click to the right of the whole line of text.
    textInput.moveCursorTo(Vec2D(maxWidth + 20.0f, secondLine.y()));
    CHECK(textInput.cursor().start().codePointIndex() ==
          secondLine.lastCodePointIndex(textInput.shape().glyphLookup()));
}

TEST_CASE("cursor is placed correctly with mixed bidi paragraphs",
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

    CHECK(textInput.cursor().start().codePointIndex() ==
          secondLine.firstCodePointIndex(textInput.shape().glyphLookup()));

    // Click to the right of the whole line of text.
    textInput.moveCursorTo(Vec2D(maxWidth + 20.0f, secondLine.y()));
    CHECK(textInput.cursor().start().codePointIndex() ==
          secondLine.lastCodePointIndex(textInput.shape().glyphLookup()));
}

TEST_CASE("cursor moves correctly", "[text_input]")
{
    auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");

    RawTextInput textInput;
    std::string defaultText =
        "this is some\nmultiline text input\nwith one final line";
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
    std::string defaultText =
        "caf\xC3\xA9s"; // UTF-8 for café with precomposed é
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
    CHECK(A.start().codePointIndex() == START);                                \
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
static void makeAlignedInput(RawTextInput& textInput,
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
        CHECK(textInput.cursorVisualPosition(CursorPosition::zero()).x() ==
              0.0f);
        CHECK(textInput.bounds().minX == 0.0f);
        leftEndX = textInput.cursorVisualPosition(CursorPosition(0, 5)).x();
        CHECK(leftEndX == Approx(naturalWidth));
    }

    {
        RawTextInput textInput;
        makeAlignedInput(textInput, font, TextAlign::right, alignWidth);
        textInput.update(&factory);

        CHECK(textInput.cursorVisualPosition(CursorPosition::zero()).x() ==
              Approx(alignWidth - naturalWidth));
        CHECK(textInput.cursorVisualPosition(CursorPosition(0, 5)).x() ==
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
        CHECK(textInput.cursorVisualPosition(CursorPosition::zero()).x() ==
              Approx(expectedStart));
        CHECK(textInput.cursorVisualPosition(CursorPosition(0, 5)).x() ==
              Approx(expectedStart + naturalWidth));
        CHECK(textInput.bounds().minX == Approx(expectedStart));
        CHECK(textInput.bounds().width() == Approx(naturalWidth));
    }
}

TEST_CASE("alignment falls back to the left when the text overflows",
          "[text_input]")
{
    auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
    NoOpFactory factory;

    // An align width narrower than the text: alignment must not shift the text,
    // so the horizontal scrolling behavior stays intact.
    for (TextAlign align :
         {TextAlign::left, TextAlign::right, TextAlign::center})
    {
        RawTextInput textInput;
        makeAlignedInput(textInput, font, align, 10.0f);
        textInput.update(&factory);

        CHECK(textInput.cursorVisualPosition(CursorPosition::zero()).x() ==
              0.0f);
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

    for (TextAlign align :
         {TextAlign::left, TextAlign::right, TextAlign::center})
    {
        RawTextInput textInput;
        makeAlignedInput(textInput, font, align, alignWidth);
        textInput.update(&factory);

        for (uint32_t index = 0; index <= 5; index++)
        {
            auto position =
                textInput.cursorVisualPosition(CursorPosition(0, index));
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
        CHECK(textInput.shape().verticalOffset() ==
              Approx((alignHeight - naturalHeight) / 2.0f));
        CHECK(textInput.bounds().minY ==
              Approx(topMinY + (alignHeight - naturalHeight) / 2.0f));
        CHECK(textInput.bounds().height() == Approx(naturalHeight));
        // The caret follows the text down by the same amount.
        CHECK(textInput.cursorVisualPosition(CursorPosition(0, 0)).top() -
                  topCaretY ==
              Approx((alignHeight - naturalHeight) / 2.0f));
    }

    {
        RawTextInput textInput;
        makeAlignedInput(textInput, font, TextAlign::left, 0.0f);
        textInput.alignHeight(alignHeight);
        textInput.verticalAlign(VerticalTextAlign::bottom);
        textInput.update(&factory);
        CHECK(textInput.shape().verticalOffset() ==
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
TEST_CASE("multiline alignment uses the align width, not the wrap width",
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
#endif
