#include "catch.hpp"
#include "rive/artboard.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout_component.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text_engine.hpp"
#include "rive_file_reader.hpp"

using namespace rive;

// A text only shapes when the styled text it would shape differs from the
// one its shape was made from: being dirtied, resized, re-shown or given a
// new direction re-breaks its lines at most.

// Each line's baseline and glyphs.
static std::vector<std::pair<float, std::vector<GlyphID>>> laidOutLines(
    Text* text)
{
  std::vector<std::pair<float, std::vector<GlyphID>>> lines;
  for (const OrderedLine& line : text->orderedLines())
  {
    std::vector<GlyphID> glyphs;
    for (auto glyph : line)
    {
      glyphs.push_back(std::get<0>(glyph)->glyphs[std::get<1>(glyph)]);
    }
    lines.emplace_back(line.y(), std::move(glyphs));
  }
  return lines;
}

// A layout that hides a text's host hands the text a 0x0 slot (Yoga zeroes
// hidden nodes), then its slot back when it shows again; the hidden text
// doesn't update in between.
TEST_CASE(
    "A text given a 0x0 slot and its slot back shapes and breaks nothing",
    "[text]")
{
  auto file = ReadRiveFile("assets/layout/layout_direction.riv");
  auto artboard = file->artboard();
  auto text = artboard->find<Text>("SampleText");
  REQUIRE(text != nullptr);
  REQUIRE(text->parent()->is<LayoutComponent>());
  auto layout = text->parent()->as<LayoutComponent>();
  artboard->advance(0.0f);
  artboard->advance(0.0f);
  const auto lines = laidOutLines(text);
  const AABB bounds = text->localBounds();
  const uint64_t shapes = Font::sm_shapeCount;
  const size_t breaks = text->lineBreakCount();

  const LayoutScaleType widthScaleType = layout->style()->widthScaleType();
  const LayoutScaleType heightScaleType = layout->style()->heightScaleType();
  const LayoutDirection direction = layout->actualDirection();
  text->controlSize(Vec2D(), widthScaleType, heightScaleType, direction);
  text->controlSize(
      Vec2D(layout->layoutWidth(), layout->layoutHeight()),
      widthScaleType,
      heightScaleType,
      direction);
  artboard->advance(0.0f);

  CHECK(Font::sm_shapeCount == shapes);
  CHECK(text->lineBreakCount() == breaks);
  CHECK(laidOutLines(text) == lines);
  CHECK(text->localBounds() == bounds);
}

TEST_CASE(
    "A new layout direction breaks the lines again without reshaping",
    "[text]")
{
  auto file = ReadRiveFile("assets/layout/layout_direction.riv");
  auto artboard = file->artboard();
  auto container = artboard->find<LayoutComponent>("TextContainer");
  auto text = artboard->find<Text>("SampleText");
  artboard->advance(0.0f);
  REQUIRE(text->align() == TextAlign::right);
  const size_t shapes = text->shapeCount();

  container->style()->directionValue(YGDirectionLTR);
  artboard->advance(0.0f);
  CHECK(text->align() == TextAlign::left);
  CHECK(text->shapeCount() == shapes);

  // As a text laid out left to right from the start.
  auto freshFile = ReadRiveFile("assets/layout/layout_direction.riv");
  auto freshArtboard = freshFile->artboard();
  freshArtboard->find<LayoutComponent>("TextContainer")
      ->style()
      ->directionValue(YGDirectionLTR);
  auto fresh = freshArtboard->find<Text>("SampleText");
  freshArtboard->advance(0.0f);
  CHECK(laidOutLines(text) == laidOutLines(fresh));
  CHECK(text->localBounds() == fresh->localBounds());
}

TEST_CASE(
    "A text dirtied without a change reshapes and breaks nothing",
    "[text]")
{
  auto file = ReadRiveFile("assets/ellipsis.riv");
  auto artboard = file->artboard();
  auto text = artboard->find<Text>()[0];
  artboard->advance(0.0f);
  const auto lines = laidOutLines(text);
  const size_t shapes = text->shapeCount();
  const size_t breaks = text->lineBreakCount();

  text->markShapeDirty();
  artboard->advance(0.0f);
  CHECK(text->shapeCount() == shapes);
  CHECK(text->lineBreakCount() == breaks);
  CHECK(laidOutLines(text) == lines);

  // A real change still shapes, once.
  auto run = artboard->find<TextValueRun>()[0];
  run->text(run->text() + " more");
  artboard->advance(0.0f);
  CHECK(text->shapeCount() == shapes + 1);
}

// The layout measures a hug text again when it comes back (hiding it wiped
// the layout's measurements), and the measure finds the shape it has.
TEST_CASE("Re-showing a hug text measures it without shaping", "[text]")
{
  auto file = ReadRiveFile("assets/layout/text_layout_7_3.riv");
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  Text* text = nullptr;
  for (auto candidate : artboard->find<Text>())
  {
    auto parent = candidate->parent();
    if (parent != nullptr && parent->is<LayoutComponent>() &&
        !parent->is<Artboard>())
    {
      text = candidate;
      break;
    }
  }
  REQUIRE(text != nullptr);
  auto layout = text->parent()->as<LayoutComponent>();
  REQUIRE(layout->style() != nullptr);
  artboard->advance(0.0f);
  artboard->advance(0.0f);
  const AABB bounds = text->localBounds();
  const uint64_t shapes = Font::sm_shapeCount;

  layout->style()->displayValue(1); // none
  artboard->advance(0.0f);
  artboard->advance(0.0f);
  layout->style()->displayValue(0); // flex
  artboard->advance(0.0f);
  artboard->advance(0.0f);

  CHECK(Font::sm_shapeCount == shapes);
  CHECK(text->localBounds() == bounds);
}

TEST_CASE("A fitted text lays out the shape its fit found", "[text]")
{
  auto file = ReadRiveFile("assets/ellipsis.riv");
  auto artboard = file->artboard();
  auto text = artboard->find<Text>()[0];
  text->overflow(TextOverflow::visible);
  artboard->advance(0.0f);
  const float width = text->width();
  const float height = text->height();
  const float authoredSize = text->shape()[0].runs[0].size;

  // Too small for the authored size: the search shapes its candidates, and
  // the update takes the one that fit.
  size_t updates = text->shapeCount(Text::ShapeSource::update);
  size_t fits = text->shapeCount(Text::ShapeSource::fit);
  text->overflow(TextOverflow::fitFontSize);
  artboard->advance(0.0f);
  CHECK(text->shape()[0].runs[0].size < authoredSize);
  CHECK(text->shapeCount(Text::ShapeSource::fit) > fits);
  CHECK(text->shapeCount(Text::ShapeSource::update) == updates);

  // Large enough for it: one candidate, which the update takes.
  text->width(width * 4.0f);
  text->height(height * 4.0f);
  fits = text->shapeCount(Text::ShapeSource::fit);
  artboard->advance(0.0f);
  CHECK(text->shape()[0].runs[0].size == authoredSize);
  CHECK(text->shapeCount(Text::ShapeSource::fit) == fits + 1);
  CHECK(text->shapeCount(Text::ShapeSource::update) == updates);

  // Still large enough: the candidate is the shape it has.
  const size_t shapes = text->shapeCount();
  text->width(width * 5.0f);
  artboard->advance(0.0f);
  CHECK(text->shapeCount() == shapes);
}

static rcp<Font> loadFont(const char* path)
{
  FILE* fp = fopen(path, "rb");
  REQUIRE(fp != nullptr);
  fseek(fp, 0, SEEK_END);
  const size_t length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  std::vector<uint8_t> bytes(length);
  REQUIRE(fread(bytes.data(), 1, length, fp) == length);
  fclose(fp);
  return HBFont::Decode(bytes);
}

static rcp<Font> gFallback;
static rcp<Font> pickFallback(const Unichar, uint32_t index, const Font*)
{
  return index == 0 ? gFallback : nullptr;
}

// Whether any of the text's glyphs come from [font].
static bool drawsWith(Text* text, const Font* font)
{
  for (const Paragraph& paragraph : text->shape())
  {
    for (const GlyphRun& run : paragraph.runs)
    {
      if (run.font.get() == font)
      {
        return true;
      }
    }
  }
  return false;
}

// Shaping reads the fallback fonts, which hosts change on their own; a
// shape made before they changed is made again.
TEST_CASE("A text reshapes after the fallback fonts change", "[text]")
{
  Font::FallbackProc previous = Font::gFallbackProc;
  Font::gFallbackProc = nullptr;
  auto file = ReadRiveFile("assets/layout/layout_direction.riv");
  auto artboard = file->artboard();
  auto text = artboard->find<Text>("SampleText");
  artboard->find<TextValueRun>()[0]->text("مرحبا بالعالم");
  artboard->advance(0.0f);
  auto arabic = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
  REQUIRE_FALSE(drawsWith(text, arabic.get()));

  // A new proc.
  gFallback = arabic;
  Font::gFallbackProc = pickFallback;
  size_t shapes = text->shapeCount();
  text->markShapeDirty();
  artboard->advance(0.0f);
  CHECK(text->shapeCount() == shapes + 1);
  CHECK(drawsWith(text, arabic.get()));

  // The same proc picking from other fonts, which the host reports.
  auto other = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
  gFallback = other;
  Font::fallbackFontsChanged();
  shapes = text->shapeCount();
  text->markShapeDirty();
  artboard->advance(0.0f);
  CHECK(text->shapeCount() == shapes + 1);
  CHECK(drawsWith(text, other.get()));

  Font::gFallbackProc = previous;
  gFallback = nullptr;
}

// A new size only breaks the lines again, unless the shape was made under
// fallback fonts that have since changed.
TEST_CASE("A text resized after the fallback fonts change reshapes", "[text]")
{
  Font::FallbackProc previous = Font::gFallbackProc;
  Font::gFallbackProc = nullptr;
  auto file = ReadRiveFile("assets/layout/layout_direction.riv");
  auto artboard = file->artboard();
  auto text = artboard->find<Text>("SampleText");
  REQUIRE(text->parent()->is<LayoutComponent>());
  auto layout = text->parent()->as<LayoutComponent>();
  artboard->find<TextValueRun>()[0]->text("مرحبا بالعالم");
  artboard->advance(0.0f);
  auto arabic = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
  REQUIRE_FALSE(drawsWith(text, arabic.get()));

  gFallback = arabic;
  Font::gFallbackProc = pickFallback;
  const size_t shapes = text->shapeCount();
  text->controlSize(
      Vec2D(layout->layoutWidth() / 2.0f, layout->layoutHeight()),
      layout->style()->widthScaleType(),
      layout->style()->heightScaleType(),
      layout->actualDirection());
  artboard->advance(0.0f);
  CHECK(text->shapeCount() == shapes + 1);
  CHECK(drawsWith(text, arabic.get()));

  Font::gFallbackProc = previous;
  gFallback = nullptr;
}
