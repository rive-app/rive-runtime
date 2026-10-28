#include "catch.hpp"
#include "rive/artboard.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout_component.hpp"
#include "rive/text/cursor.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text.hpp"
#include "rive/text/raw_text_input.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text/utf.hpp"
#include "rive/text_engine.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_factory.hpp"
#include <cstring>

using namespace rive;

static rcp<Font> loadArabicFont()
{
  FILE* fp = fopen("assets/IBMPlexSansArabic-Regular.ttf", "rb");
  REQUIRE(fp != nullptr);
  fseek(fp, 0, SEEK_END);
  const size_t length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  std::vector<uint8_t> bytes(length);
  REQUIRE(fread(bytes.data(), 1, length, fp) == length);
  fclose(fp);
  return HBFont::Decode(bytes);
}

struct EllipsizedLine
{
  SimpleArray<Paragraph> shape;
  SimpleArray<SimpleArray<GlyphLine>> lines;
  GlyphRun ellipsisRun;
  std::unique_ptr<OrderedLine> line;
};

// Shapes a single paragraph that overflows a 120 wide, non wrapping line and
// ellipsizes it.
static void ellipsize(EllipsizedLine& result, const char* utf8, TextAlign align)
{
  auto font = loadArabicFont();
  std::vector<Unichar> unichars;
  const uint8_t* ptr = (const uint8_t*)utf8;
  const uint8_t* end = ptr + strlen(utf8);
  while (ptr < end)
  {
    unichars.push_back(UTF::NextUTF8(&ptr, end));
  }
  TextRun run = {font, 20.0f, -1.0f, 0.0f, (uint32_t)unichars.size(), 0, 0};
  result.shape = font->shapeText(unichars, Span<TextRun>(&run, 1));
  result
      .lines = Text::BreakLines(result.shape, 120.0f, align, TextWrap::noWrap);
  REQUIRE(result.shape.size() == 1);
  REQUIRE(result.lines[0].size() == 1);
  result.line = std::make_unique<OrderedLine>(
      result.shape[0],
      result.lines[0][0],
      120.0f,
      align,
      true,
      true,
      &result.ellipsisRun,
      0.0f);
}

// Returns the [min, max] x the line's glyphs cover.
static std::pair<float, float> glyphSpan(const OrderedLine& line)
{
  float x = line.startX();
  float minX = x;
  for (auto [run, glyphIndex] : line)
  {
    x += run->advances[glyphIndex];
  }
  return {minX, x};
}

TEST_CASE("ellipsis sits at the end edge of a right to left line", "[text]")
{
  EllipsizedLine result;
  ellipsize(result, "مرحبا بالعالم هذا نص طويل جدا", TextAlign::left);
  const OrderedLine& line = *result.line;
  REQUIRE(result.shape[0].baseDirection() == TextDirection::rtl);
  REQUIRE(line.runs().size() == 2);
  // Visually first, so drawn on the left, past the end of the kept text.
  REQUIRE(line.runs().front() == &result.ellipsisRun);
  REQUIRE(result.ellipsisRun.dir() == TextDirection::rtl);
  // The kept text is still the logical start, drawn rightmost.
  auto lastGlyph = line.begin();
  for (auto itr = line.begin(); itr != line.end(); ++itr)
  {
    lastGlyph = itr;
  }
  REQUIRE(lastGlyph.run()->textIndices[lastGlyph.glyphIndex()] == 0);
}

TEST_CASE("ellipsis sits at the end edge of a left to right line", "[text]")
{
  EllipsizedLine result;
  ellipsize(result, "Hello world this is long", TextAlign::left);
  const OrderedLine& line = *result.line;
  REQUIRE(line.runs().back() == &result.ellipsisRun);
  REQUIRE(result.ellipsisRun.dir() == TextDirection::ltr);
}

TEST_CASE("ellipsis follows the paragraph, not the run it cuts", "[text]")
{
  {
    // Right to left paragraph cut inside its trailing English.
    EllipsizedLine result;
    ellipsize(result, "مرحبا Hello World Again", TextAlign::right);
    REQUIRE(result.shape[0].baseDirection() == TextDirection::rtl);
    REQUIRE(result.line->runs().front() == &result.ellipsisRun);
  }
  {
    // Left to right paragraph cut inside its trailing Arabic.
    EllipsizedLine result;
    ellipsize(result, "Hello مرحبا بالعالم هذا", TextAlign::left);
    REQUIRE(result.shape[0].baseDirection() == TextDirection::ltr);
    REQUIRE(result.line->runs().back() == &result.ellipsisRun);
  }
}

TEST_CASE("an ellipsized line is aligned by its ellipsized width", "[text]")
{
  const char* text = "مرحبا بالعالم هذا نص طويل جدا";
  {
    EllipsizedLine result;
    ellipsize(result, text, TextAlign::right);
    // Aligned for the full line, which starts well outside the box.
    REQUIRE(result.lines[0][0].startX < 0.0f);
    auto span = glyphSpan(*result.line);
    CHECK(span.first >= 0.0f);
    CHECK(span.second == Approx(120.0f));
  }
  {
    EllipsizedLine result;
    ellipsize(result, text, TextAlign::center);
    auto span = glyphSpan(*result.line);
    CHECK(span.first >= 0.0f);
    CHECK(span.first == Approx(120.0f - span.second));
  }
  {
    EllipsizedLine result;
    ellipsize(result, text, TextAlign::left);
    auto span = glyphSpan(*result.line);
    CHECK(span.first == 0.0f);
    CHECK(span.second <= 120.0f);
  }
}

static rcp<Font> gArabicFallback;
static rcp<Font> pickArabicFallback(const Unichar, uint32_t index, const Font*)
{
  return index == 0 ? gArabicFallback : nullptr;
}

// The fixtures only carry a Latin font, so draw Arabic from a fallback while
// in scope.
struct ArabicFallback
{
  Font::FallbackProc previous = Font::gFallbackProc;
  ArabicFallback()
  {
    gArabicFallback = loadArabicFont();
    Font::gFallbackProc = pickArabicFallback;
  }
  ~ArabicFallback()
  {
    Font::gFallbackProc = previous;
    gArabicFallback = nullptr;
  }
};

// The ellipsis is the one run the shape doesn't own.
static bool isEllipsis(Text* text, const GlyphRun* run)
{
  const SimpleArray<GlyphRun>& shapedRuns = text->shape()[0].runs;
  return run < shapedRuns.data() ||
         run >= shapedRuns.data() + shapedRuns.size();
}

TEST_CASE("an ellipsized text stays inside its box", "[text]")
{
  ArabicFallback fallback;

  struct Case
  {
    const char* text;
    TextAlign align;
    TextWrap wrap;
  };
  const char* ltr = "one two three four five six seven";
  const char* rtl = "مرحبا بالعالم هذا نص طويل جدا جدا";
  Case cases[] = {
      {ltr, TextAlign::left, TextWrap::noWrap},
      {ltr, TextAlign::right, TextWrap::noWrap},
      {ltr, TextAlign::center, TextWrap::noWrap},
      {rtl, TextAlign::left, TextWrap::noWrap},
      {rtl, TextAlign::right, TextWrap::noWrap},
      {rtl, TextAlign::right, TextWrap::wrap},
  };
  for (const Case& c : cases)
  {
    auto file = ReadRiveFile("assets/ellipsis.riv");
    auto artboard = file->artboard();
    auto text = artboard->find<rive::Text>()[0];
    artboard->find<rive::TextValueRun>()[0]->text(c.text);
    text->alignValue((uint32_t)c.align);
    text->wrapValue((uint32_t)c.wrap);
    artboard->advance(0.0f);

    REQUIRE(text->orderedLines().size() == 1);
    const OrderedLine& line = text->orderedLines()[0];
    auto span = glyphSpan(line);
    CHECK(span.first >= 0.0f);
    CHECK(span.second <= text->width() + 0.001f);
    if (c.align == TextAlign::right)
    {
      CHECK(span.second == Approx(text->width()));
    }
    if (c.text == rtl)
    {
      CHECK(isEllipsis(text, line.runs().front()));
    }
    else
    {
      CHECK(isEllipsis(text, line.runs().back()));
    }
  }
}

// A layout sizes its text and, by its direction, aligns it to its start edge.
// The ellipsis still follows the text's own direction.
TEST_CASE(
    "an ellipsized text in a layout aligns to the layout's direction",
    "[text]")
{
  ArabicFallback fallback;

  const char* ltr = "one two three four five six seven eight nine ten";
  const char* rtl = "مرحبا بالعالم هذا نص طويل جدا جدا جدا جدا جدا";
  for (bool rtlLayout : {false, true})
  {
    for (const char* content : {ltr, rtl})
    {
      auto file = ReadRiveFile("assets/layout/layout_direction.riv");
      auto artboard = file->artboard();
      artboard->find<LayoutComponent>("TextContainer")
          ->style()
          ->directionValue(rtlLayout ? YGDirectionRTL : YGDirectionLTR);
      auto text = artboard->find<rive::Text>("SampleText");
      artboard->find<rive::TextValueRun>()[0]->text(content);
      text->overflowValue((uint32_t)TextOverflow::ellipsis);
      text->wrapValue((uint32_t)TextWrap::noWrap);
      artboard->advance(0.0f);

      CHECK(text->align() == (rtlLayout ? TextAlign::right : TextAlign::left));
      REQUIRE(text->orderedLines().size() == 1);
      const OrderedLine& line = text->orderedLines()[0];
      CHECK(isEllipsis(
          text,
          content == rtl ? line.runs().front() : line.runs().back()));
      float width = text->effectiveWidth();
      auto span = glyphSpan(line);
      CHECK(span.first >= 0.0f);
      CHECK(span.second <= width + 0.001f);
      if (rtlLayout)
      {
        CHECK(span.second == Approx(width));
      }
      else
      {
        CHECK(span.first == 0.0f);
      }
    }
  }
}

// Whatever reports where an ellipsized line's glyphs are has to agree with
// where it draws them, not with where the line breaker put the full line.
TEST_CASE("an ellipsized line reports where it draws", "[text]")
{
  const char* content = "one two three four five six seven eight nine ten";
  NoOpFactory factory;

  SECTION("RawText pen positions")
  {
    RawText text(&factory);
    text.append(content, nullptr, loadArabicFont(), 20.0f);
    text.sizingIndex((int)TextSizing::fixed);
    text.overflowIndex((int)TextOverflow::ellipsis);
    text.wrapIndex((int)TextWrap::noWrap);
    text.alignIndex((int)TextAlign::right);
    text.maxWidth(120.0f);
    text.maxHeight(100.0f);
    REQUIRE(text.lineCount() == 1);

    bool first = true;
    float penStart = 0.0f, penEnd = 0.0f;
    text.forEachGlyph(0, [&](const GlyphRun& run, uint32_t index, Vec2D at) {
      float pen = at.x - run.offsets[index].x;
      if (first)
      {
        penStart = pen;
        first = false;
      }
      penEnd = pen + run.advances[index];
      return true;
    });
    CHECK(penStart == Approx(text.orderedLines()[0].startX()));
    CHECK(penStart >= 0.0f);
    CHECK(penEnd == Approx(120.0f));
  }

  SECTION("RawTextInput caret and selection")
  {
    RawTextInput input;
    input.font(loadArabicFont());
    input.fontSize(20.0f);
    input.sizing(TextSizing::fixed);
    input.overflow(TextOverflow::ellipsis);
    input.align(TextAlign::right);
    input.maxWidth(120.0f);
    // One line tall, so the first wrapped line takes the ellipsis.
    input.maxHeight(30.0f);
    input.text(content);
    input.update(&factory);

    const OrderedLine& line = input.shape().orderedLines()[0];
    // The ellipsis changed the line's width, or this proves nothing.
    REQUIRE(line.startX() != Approx(line.glyphLine().startX));

    // Left to right, so the first code point is the line's leftmost glyph.
    CHECK(
        input.cursorVisualPosition(CursorPosition(0, 0)).x() ==
        Approx(line.startX()));

    std::vector<AABB> rects;
    Cursor(CursorPosition(0, 0), CursorPosition(0, 3))
        .selectionRects(rects, input.shape());
    REQUIRE(!rects.empty());
    CHECK(rects.front().minX == Approx(line.startX()));
  }
}
