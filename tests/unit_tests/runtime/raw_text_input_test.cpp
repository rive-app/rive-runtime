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
#endif