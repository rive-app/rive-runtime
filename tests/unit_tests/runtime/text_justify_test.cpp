#include "catch.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/text/cursor.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text.hpp"
#include "rive/text/raw_text_input.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text_engine.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_factory.hpp"
#include <cstdio>
#include <vector>

using namespace rive;

namespace
{
rcp<Font> loadFont(const char* filename)
{
  FILE* fp = fopen(filename, "rb");
  REQUIRE(fp != nullptr);
  fseek(fp, 0, SEEK_END);
  std::vector<uint8_t> bytes(ftell(fp));
  fseek(fp, 0, SEEK_SET);
  REQUIRE(fread(bytes.data(), 1, bytes.size(), fp) == bytes.size());
  fclose(fp);
  return HBFont::Decode(bytes);
}

const char* kLatin = "assets/fonts/Inter_18pt-Regular.ttf";
const char* kArabic = "assets/IBMPlexSansArabic-Regular.ttf";
const char* kWords = "one two three four five six seven eight nine ten eleven";
constexpr float kWidth = 150.0f;

// Where a line's glyphs start and end, as the line places them.
std::pair<float, float> extent(const OrderedLine& line)
{
  float x = line.startX();
  for (auto glyph : line)
  {
    x += line.advance(*std::get<0>(glyph), std::get<1>(glyph));
  }
  return {line.startX(), x};
}

rcp<RenderPaint> makePaint(NoOpFactory& factory)
{
  return static_cast<Factory&>(factory).makeRenderPaint();
}

struct JustifiedText
{
  NoOpFactory factory;
  RawText text{&factory};

  JustifiedText(const char* content, const char* font = kLatin)
  {
    text.append(content, makePaint(factory), loadFont(font), 20.0f);
    text.sizing(TextSizing::autoHeight);
    text.maxWidth(kWidth);
    text.align(TextAlign::justify);
  }
};
} // namespace

TEST_CASE("justify stretches every line but a paragraph's last", "[text]")
{
  JustifiedText justified(kWords);
  RawText& text = justified.text;
  REQUIRE(text.lineCount() >= 3);
  const auto& lines = text.orderedLines();
  for (size_t i = 0; i + 1 < lines.size(); i++)
  {
    INFO("line " << i);
    CHECK(lines[i].justifyGap() > 0.0f);
    auto span = extent(lines[i]);
    CHECK(span.first == 0.0f);
    CHECK(span.second == Approx(kWidth));
  }
  // The last line keeps its width and starts where the paragraph does.
  CHECK(lines.back().justifyGap() == 0.0f);
  auto last = extent(lines.back());
  CHECK(last.first == 0.0f);
  CHECK(last.second < kWidth - 1.0f);
}

TEST_CASE("justify widens only the spaces", "[text]")
{
  JustifiedText justified(kWords);
  RawText& text = justified.text;
  const OrderedLine& line = text.orderedLines()[0];
  float gap = line.justifyGap();
  REQUIRE(gap > 0.0f);
  int widened = 0;
  for (auto glyph : line)
  {
    const GlyphRun& run = *std::get<0>(glyph);
    uint32_t index = std::get<1>(glyph);
    float added = line.advance(run, index) - run.advances[index];
    if (added != 0.0f)
    {
      CHECK(added == Approx(gap));
      widened++;
    }
  }
  CHECK(widened > 0);

  // Scripts read the same positions: each glyph's pen plus its advance is
  // the next one's.
  float expected = line.startX();
  text.forEachGlyph(0, [&](const GlyphRun& run, uint32_t index, Vec2D at) {
    CHECK(at.x - run.offsets[index].x == Approx(expected));
    expected += text.glyphAdvance(0, run, index);
    return true;
  });
  CHECK(expected == Approx(kWidth));
}

TEST_CASE("justified right to left lines fill the width too", "[text]")
{
  JustifiedText justified(
      "مرحبا بالعالم هذا نص طويل جدا جدا جدا جدا جدا",
      kArabic);
  RawText& text = justified.text;
  REQUIRE(text.lineCount() >= 2);
  REQUIRE(text.lineDirection(0) == TextDirection::rtl);
  const auto& lines = text.orderedLines();
  for (size_t i = 0; i + 1 < lines.size(); i++)
  {
    auto span = extent(lines[i]);
    CHECK(span.first == 0.0f);
    CHECK(span.second == Approx(kWidth));
  }
  // The last line starts where a right to left paragraph starts.
  auto last = extent(lines.back());
  CHECK(last.second == Approx(kWidth));
  CHECK(last.first > 1.0f);
}

TEST_CASE("a justified line without spaces stays as it is", "[text]")
{
  NoOpFactory factory;
  RawText text(&factory);
  text.append(
      "abcdefghijklmnopqrstuvwxyzabcdefghij end",
      makePaint(factory),
      loadFont(kLatin),
      20.0f);
  text.sizing(TextSizing::autoHeight);
  text.maxWidth(kWidth);
  text.wordBreak(TextWordBreak::breakAll);
  text.align(TextAlign::justify);
  REQUIRE(text.lineCount() >= 2);
  const OrderedLine& first = text.orderedLines()[0];
  CHECK(first.justifyGap() == 0.0f);
  CHECK(extent(first).first == 0.0f);
}

TEST_CASE("text that doesn't wrap isn't justified", "[text]")
{
  NoOpFactory factory;
  RawText text(&factory);
  text.append("one two\nthree four", makePaint(factory), loadFont(kLatin), 20);
  text.align(TextAlign::justify);
  // autoWidth: every line ends its paragraph.
  REQUIRE(text.lineCount() == 2);
  for (const OrderedLine& line : text.orderedLines())
  {
    CHECK(line.justifyGap() == 0.0f);
    CHECK(line.startX() == 0.0f);
  }
}

TEST_CASE("scripts name justify as ordinal 5", "[text]")
{
  NoOpFactory factory;
  RawText text(&factory);
  text.alignIndex(5);
  CHECK(text.align() == TextAlign::justify);
  CHECK(text.alignIndex() == 5);
  text.alignIndex(3);
  CHECK(text.alignIndex() == 3);
  text.alignIndex(2);
  CHECK(text.align() == TextAlign::center);
  CHECK(text.alignIndex() == 2);
}

TEST_CASE("carets and selections follow justified glyphs", "[text]")
{
  JustifiedText justified(kWords);
  RawText& text = justified.text;
  TextLayoutView view = text.layoutView();

  // The caret before each glyph of the first line sits at the glyph's pen.
  text.forEachGlyph(0, [&](const GlyphRun& run, uint32_t index, Vec2D at) {
    auto caret = CursorPosition::atIndex(run.textIndices[index], view)
                     .visualPosition(view);
    REQUIRE(caret.found());
    CHECK(caret.x() == Approx(at.x - run.offsets[index].x).margin(0.01f));
    return true;
  });

  // Selecting the first line covers the whole width.
  uint32_t lastIndex = 0;
  text.forEachGlyph(0, [&](const GlyphRun& run, uint32_t index, Vec2D) {
    lastIndex = std::max(lastIndex, run.textIndices[index]);
    return true;
  });
  std::vector<AABB> rects;
  Cursor(
      CursorPosition::atIndex(0, view),
      CursorPosition::atIndex(lastIndex + 1, view))
      .selectionRects(rects, view);
  REQUIRE(!rects.empty());
  float maxX = 0.0f;
  for (const AABB& rect : rects)
  {
    maxX = std::max(maxX, rect.maxX);
  }
  CHECK(maxX == Approx(kWidth));

  // A point on the right edge of the first line hits its last glyph.
  const OrderedLine& line = text.orderedLines()[0];
  auto hit = CursorPosition::fromTranslation(
      Vec2D(kWidth - 0.5f, line.y()),
      view);
  CHECK(hit.codePointIndex() >= lastIndex);
}

TEST_CASE("a justified text input fills its width", "[text]")
{
  NoOpFactory factory;
  RawTextInput input;
  input.font(loadFont(kLatin));
  input.fontSize(20.0f);
  input.sizing(TextSizing::autoHeight);
  input.maxWidth(kWidth);
  input.align(TextAlign::justify);
  input.text(kWords);
  input.update(&factory);

  const auto& lines = input.shape().orderedLines();
  REQUIRE(lines.size() >= 3);
  for (size_t i = 0; i + 1 < lines.size(); i++)
  {
    auto span = extent(lines[i]);
    CHECK(span.first == 0.0f);
    CHECK(span.second == Approx(kWidth));
  }
  CHECK(lines.back().justifyGap() == 0.0f);
}

TEST_CASE("a justified Text fills its box", "[text]")
{
  auto file = ReadRiveFile("assets/ellipsis.riv");
  auto artboard = file->artboard();
  auto text = artboard->find<rive::Text>()[0];
  artboard->find<rive::TextValueRun>()[0]->text(kWords);
  text->alignValue((uint32_t)TextAlign::justify);
  text->overflowValue((uint32_t)TextOverflow::visible);
  text->wrapValue((uint32_t)TextWrap::wrap);
  artboard->advance(0.0f);

  const auto& lines = text->orderedLines();
  REQUIRE(lines.size() >= 2);
  float width = text->effectiveWidth();
  for (size_t i = 0; i + 1 < lines.size(); i++)
  {
    INFO("line " << i);
    auto span = extent(lines[i]);
    CHECK(span.first == 0.0f);
    CHECK(span.second == Approx(width));
  }
  CHECK(lines.back().justifyGap() == 0.0f);
}

namespace
{
struct JustifiedInput
{
  NoOpFactory factory;
  RawTextInput input;

  JustifiedInput(const char* content)
  {
    input.font(loadFont(kLatin));
    input.fontSize(20.0f);
    input.sizing(TextSizing::autoHeight);
    input.maxWidth(kWidth);
    input.align(TextAlign::justify);
    input.text(content);
    input.update(&factory);
  }

  // Checks every justified line widens exactly its spaces in the input's
  // current text and fills width; returns how many lines were justified.
  int checkJustified(float width)
  {
    std::string text = input.text();
    int justified = 0;
    for (const OrderedLine& line : input.shape().orderedLines())
    {
      if (line.justifyGap() == 0.0f)
      {
        continue;
      }
      justified++;
      for (auto glyph : line)
      {
        const GlyphRun& run = *std::get<0>(glyph);
        uint32_t index = std::get<1>(glyph);
        uint32_t textIndex = run.textIndices[index];
        REQUIRE(textIndex < text.size());
        INFO("text index " << textIndex);
        CHECK(
            (line.advance(run, index) != run.advances[index]) ==
            (text[textIndex] == ' '));
      }
      auto span = extent(line);
      CHECK(span.first == 0.0f);
      CHECK(span.second == Approx(width));
    }
    return justified;
  }
};
} // namespace

TEST_CASE("an edit keeps a justified text input's other lines", "[text]")
{
  // The edited paragraph stays under half the text, or the input shapes
  // all of it again.
  JustifiedInput justified(
      "one two three\n"
      "seven eight nine ten eleven twelve thirteen\n"
      "fourteen fifteen sixteen seventeen eighteen nineteen twenty");
  RawTextInput& input = justified.input;
  int before = justified.checkJustified(kWidth);
  REQUIRE(before >= 2);

  // Long enough that the paragraphs after it move past where the old text
  // ended, and its buffer grows.
  input.cursor(Cursor::collapsed(CursorPosition(0)));
  input.insert("a run of extra words that pushes the rest of it along ");
  input.update(&justified.factory);
  // The lines of the last two paragraphs were kept, not laid out again.
  REQUIRE_FALSE(input.shape().lineChange().all);
  CHECK(input.shape().lineChange().first == 0);
  CHECK(justified.checkJustified(kWidth) > before);
}

TEST_CASE("a justified text input's bounds hold its stretched lines", "[text]")
{
  JustifiedInput justified(kWords);
  RawTextInput& input = justified.input;
  REQUIRE(input.shape().lineCount() >= 3);
  AABB bounds = input.bounds();
  CHECK(bounds.minX == 0.0f);
  CHECK(bounds.maxX == Approx(kWidth));
  AABB measured = input.measure(kWidth, 1000.0f);
  CHECK(measured.maxX == Approx(kWidth));

  // Lines that end their paragraph keep their own width.
  input.text("one two\nthree");
  input.update(&justified.factory);
  CHECK(input.bounds().maxX < kWidth - 1.0f);

  // As left aligned lines do.
  input.text(kWords);
  input.align(TextAlign::left);
  input.update(&justified.factory);
  CHECK(input.bounds().maxX < kWidth - 1.0f);
}

TEST_CASE("justified text input lines fill the field", "[text]")
{
  // A hugging field wraps at the text's width but aligns in its own.
  constexpr float kField = 200.0f;
  JustifiedInput justified(kWords);
  RawTextInput& input = justified.input;
  input.alignWidth(kField);
  input.update(&justified.factory);
  CHECK(justified.checkJustified(kField) >= 2);
  CHECK(input.bounds().maxX == Approx(kField));
}
