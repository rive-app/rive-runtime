#include "rive/math/math_types.hpp"
#include "rive/shapes/paint/dash_path.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_style_decoration.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "utils/no_op_renderer.hpp"

#include <catch.hpp>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace rive;

namespace
{
std::vector<uint8_t> loadBytes(const char* filename)
{
  FILE* fp = fopen(filename, "rb");
  REQUIRE(fp != nullptr);
  fseek(fp, 0, SEEK_END);
  const size_t length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  std::vector<uint8_t> bytes(length);
  REQUIRE(fread(bytes.data(), 1, length, fp) == length);
  fclose(fp);
  return bytes;
}

rcp<Font> loadFont(const char* filename)
{
  return HBFont::Decode(loadBytes(filename));
}

// Overwrites the big endian int16 at offset in an OpenType font's tag table.
void patchTableInt16(
    std::vector<uint8_t>& font,
    const char* tag,
    size_t offset,
    int16_t value)
{
  REQUIRE(font.size() >= 12);
  const size_t tableCount = (font[4] << 8) | font[5];
  for (size_t i = 0; i < tableCount; i++)
  {
    const size_t record = 12 + i * 16;
    REQUIRE(record + 16 <= font.size());
    if (memcmp(&font[record], tag, 4) != 0)
    {
      continue;
    }
    const size_t at = (((size_t)font[record + 8] << 24) |
                       ((size_t)font[record + 9] << 16) |
                       ((size_t)font[record + 10] << 8) | font[record + 11]) +
                      offset;
    REQUIRE(at + 2 <= font.size());
    font[at] = (uint8_t)((uint16_t)value >> 8);
    font[at + 1] = (uint8_t)value;
    return;
  }
  FAIL("the font has no " << tag << " table");
}
} // namespace

TEST_CASE("fonts report underline and strikeout metrics", "[text]")
{
  // Inter: unitsPerEm 2048, post underlinePosition -317 / thickness 140,
  // OS/2 yStrikeoutSize 140 / yStrikeoutPosition 663.
  auto inter = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(inter != nullptr);
  const Font::LineMetrics& metrics = inter->lineMetrics();
  CHECK(metrics.underlinePosition == Approx(317.0f / 2048.0f));
  CHECK(metrics.underlineThickness == Approx(140.0f / 2048.0f));
  CHECK(metrics.strikeoutPosition == Approx(-663.0f / 2048.0f));
  CHECK(metrics.strikeoutThickness == Approx(140.0f / 2048.0f));
  // Size-scaled accessors.
  CHECK(inter->underlineThickness(2048.0f) == Approx(140.0f));
  CHECK(inter->strikeoutPosition(2048.0f) == Approx(-663.0f));

  // Montserrat: unitsPerEm 1000, post -100 / 50, OS/2 50 / 310.
  auto montserrat = loadFont("assets/Montserrat.ttf");
  REQUIRE(montserrat != nullptr);
  CHECK(montserrat->underlinePosition(1000.0f) == Approx(100.0f));
  CHECK(montserrat->underlineThickness(1000.0f) == Approx(50.0f));
  CHECK(montserrat->strikeoutPosition(1000.0f) == Approx(-310.0f));
  CHECK(montserrat->strikeoutThickness(1000.0f) == Approx(50.0f));

  // A variation instance keeps them: they're re-read from the same tables
  // (MVAR deltas aren't applied). RobotoFlex: unitsPerEm 2048, post
  // -183 / 166, OS/2 102 / 512.
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  REQUIRE(roboto != nullptr);
  Font::Coord bold = {2003265652 /* wght */, 700.0f};
  auto boldRoboto = roboto->withOptions(Span<const Font::Coord>(&bold, 1), {});
  REQUIRE(boldRoboto != nullptr);
  CHECK(boldRoboto->underlinePosition(2048.0f) == Approx(183.0f));
  CHECK(boldRoboto->underlineThickness(2048.0f) == Approx(166.0f));
  CHECK(boldRoboto->strikeoutPosition(2048.0f) == Approx(-512.0f));
  CHECK(boldRoboto->strikeoutThickness(2048.0f) == Approx(102.0f));
}

TEST_CASE("a font without a strikeout falls back to 1/18 em", "[text]")
{
  // Inter with OS/2 yStrikeoutSize zeroed: its post underline (140 / 2048
  // em) still reads, and the strikeout falls back on its own.
  auto bytes = loadBytes("assets/fonts/Inter_18pt-Regular.ttf");
  patchTableInt16(bytes, "OS/2", 26, 0);
  auto inter = HBFont::Decode(bytes);
  REQUIRE(inter != nullptr);
  const Font::LineMetrics& metrics = inter->lineMetrics();
  CHECK(metrics.underlineThickness == Approx(140.0f / 2048.0f));
  CHECK(metrics.strikeoutThickness == Approx(1.0f / 18.0f));
  CHECK(
      metrics.strikeoutPosition ==
      Approx(metrics.xHeight * 0.5f - 0.5f / 18.0f));
}

TEST_CASE("an underline joins its glyphs below the baseline", "[text]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  const float size = 20.0f;
  const float baseline = 30.0f;

  TextStyleDecoration underline;
  REQUIRE(underline.decorationType() == TextDecorationType::underline);
  REQUIRE(underline.thickness() == -1.0f);
  REQUIRE(!underline.drawsOverGlyphs());
  underline.resetPath();
  underline.addGlyph(0.0f, 10.0f, baseline, font.get(), size);
  underline.addGlyph(10.0f, 25.0f, baseline, font.get(), size);
  underline.addGlyph(25.0f, 30.0f, baseline, font.get(), size);
  underline.updatePath();

  ShapePaintPath* path = underline.localPath();
  REQUIRE(path->numContours() == 1);
  AABB bounds = path->rawPath()->bounds();
  const float top = baseline + font->underlinePosition(size);
  CHECK(bounds.left() == Approx(0.0f));
  CHECK(bounds.right() == Approx(30.0f));
  CHECK(bounds.top() == Approx(top));
  CHECK(bounds.bottom() == Approx(top + font->underlineThickness(size)));
  CHECK(bounds.top() > baseline);

  SECTION("a second line adds a second bar")
  {
    underline.addGlyph(0.0f, 12.0f, baseline + 24.0f, font.get(), size);
    underline.updatePath();
    CHECK(underline.localPath()->numContours() == 2);
  }

  SECTION("an explicit thickness and offset replace the font's")
  {
    underline.thickness(3.0f);
    underline.offset(2.0f);
    underline.resetPath();
    underline.addGlyph(0.0f, 10.0f, baseline, font.get(), size);
    underline.updatePath();
    AABB moved = underline.localPath()->rawPath()->bounds();
    CHECK(moved.top() == Approx(top + 2.0f));
    CHECK(moved.height() == Approx(3.0f));
  }

  SECTION("a zero thickness draws nothing")
  {
    underline.thickness(0.0f);
    underline.resetPath();
    underline.addGlyph(0.0f, 10.0f, baseline, font.get(), size);
    underline.updatePath();
    CHECK(underline.localPath()->empty());
  }
}

TEST_CASE("a strikethrough crosses the glyphs above the baseline", "[text]")
{
  auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE(font != nullptr);
  const float size = 20.0f;
  const float baseline = 30.0f;

  TextStyleDecoration strike;
  strike.decorationTypeValue((uint8_t)TextDecorationType::strikethrough);
  REQUIRE(strike.drawsOverGlyphs());
  strike.resetPath();
  strike.addGlyph(0.0f, 40.0f, baseline, font.get(), size);
  strike.updatePath();

  AABB bounds = strike.localPath()->rawPath()->bounds();
  const float top = baseline + font->strikeoutPosition(size);
  CHECK(bounds.top() == Approx(top));
  CHECK(bounds.height() == Approx(font->strikeoutThickness(size)));
  // Between the baseline and the x-height.
  CHECK(bounds.bottom() < baseline);
  CHECK(bounds.top() > baseline + font->xHeight(size));
}

TEST_CASE("text style decoration is a shape paint container", "[text]")
{
  TextStyleDecoration decoration;
  REQUIRE(ShapePaintContainer::from(&decoration) == &decoration);
  REQUIRE(decoration.localPath() != nullptr);
  REQUIRE(decoration.localClockwisePath() == decoration.localPath());
  // Joined contours are wound clockwise, so feathered fills draw.
  REQUIRE(decoration.localPath()->fillRule() == FillRule::clockwise);
  REQUIRE(decoration.offset() == 0.0f);
}

namespace
{
class PathRecorder : public NoOpRenderer
{
public:
  std::vector<RenderPath*> paths;
  void drawPath(RenderPath* path, RenderPaint*) override
  {
    paths.push_back(path);
  }
};

struct DecorationPair
{
  TextStyleDecoration* underline = nullptr;
  TextStyleDecoration* strike = nullptr;
};

DecorationPair findDecorations(Artboard* artboard)
{
  DecorationPair pair;
  for (auto decoration : artboard->find<TextStyleDecoration>())
  {
    if (decoration->decorationType() == TextDecorationType::underline)
    {
      pair.underline = decoration;
    }
    else
    {
      pair.strike = decoration;
    }
  }
  REQUIRE(pair.underline != nullptr);
  REQUIRE(pair.strike != nullptr);
  return pair;
}
} // namespace

// assets/text_style_decoration.riv is written by rive_core's
// text_style_decoration_test.dart: three lines of one run, an underline
// (offset 1, fill + stroke) and a strikethrough (thickness 2).
TEST_CASE("editor-exported text decorations render at runtime", "[text]")
{
  auto file = ReadRiveFile("assets/text_style_decoration.riv");
  auto artboard = file->artboard();
  DecorationPair pair = findDecorations(artboard);

  REQUIRE(pair.underline->offset() == 1.0f);
  REQUIRE(pair.underline->thickness() == -1.0f);
  REQUIRE(pair.underline->shapePaints().size() == 2);
  REQUIRE(pair.strike->thickness() == 2.0f);
  auto style = pair.underline->parent()->as<TextStylePaint>();
  REQUIRE(style->decorations().size() == 2);
  REQUIRE(style->decorations()[0] == pair.underline);
  REQUIRE(style->decorations()[1] == pair.strike);

  artboard->advance(0.0f);

  // One bar per line.
  REQUIRE(pair.underline->localPath()->numContours() == 3);
  REQUIRE(pair.strike->localPath()->numContours() == 3);
  AABB underlineBounds = pair.underline->localPath()->rawPath()->bounds();
  AABB strikeBounds = pair.strike->localPath()->rawPath()->bounds();
  REQUIRE(strikeBounds.top() < underlineBounds.top());
  REQUIRE(strikeBounds.width() > 0.0f);

  // The underline's two paints draw before every glyph, the strikethrough's
  // fill after them.
  PathRecorder recorder;
  artboard->draw(&recorder);
  RenderPath* underlinePath = pair.underline->localPath()->renderPath(
      pair.underline);
  RenderPath* strikePath = pair.strike->localPath()->renderPath(pair.strike);
  REQUIRE(recorder.paths.size() > 3);
  CHECK(recorder.paths[0] == underlinePath);
  CHECK(recorder.paths[1] == underlinePath);
  CHECK(recorder.paths.back() == strikePath);
  for (size_t i = 2; i + 1 < recorder.paths.size(); i++)
  {
    CHECK(recorder.paths[i] != underlinePath);
    CHECK(recorder.paths[i] != strikePath);
  }
}

TEST_CASE("text decorations sort into the dependency graph", "[text]")
{
  auto file = ReadRiveFile("assets/text_style_decoration.riv");
  auto artboard = file->artboard();
  DecorationPair pair = findDecorations(artboard);
  auto text = pair.underline->parent()->parent()->as<Text>();
  // Like backgrounds: a Feather under a decoration's paint hangs off it, and
  // sorting after the Text means its rects are filled first.
  REQUIRE(pair.underline->graphOrder() > text->graphOrder());
  REQUIRE(pair.strike->graphOrder() > text->graphOrder());
}

TEST_CASE("decoration edits rebuild the text's decorations", "[text]")
{
  auto file = ReadRiveFile("assets/text_style_decoration.riv");
  auto artboard = file->artboard();
  DecorationPair pair = findDecorations(artboard);
  artboard->advance(0.0f);
  AABB before = pair.strike->localPath()->rawPath()->bounds();

  // A data bind or animation sets these through the generated setters.
  pair.strike->offset(5.0f);
  artboard->advance(0.0f);
  AABB after = pair.strike->localPath()->rawPath()->bounds();
  CHECK(after.top() == Approx(before.top() + 5.0f));
  CHECK(after.height() == Approx(before.height()));
}

// assets/text_style_global_decoration.riv, built with the CLI from
// assets/rml/text_style_global_decoration.rml: the Text "Line" draws its one
// run with the global style "Underlined", whose underline is 2 thick. The
// style's paint sits under its style artboard, not the Text, so an edit to
// the decoration has to reach the Text through the style's owner.
TEST_CASE("a global style's decoration follows its edits", "[text]")
{
  auto file = ReadRiveFile("assets/text_style_global_decoration.riv");
  auto artboard = file->artboard("Main")->instance();
  auto text = artboard->find<Text>("Line");
  REQUIRE(text != nullptr);
  NoOpRenderer renderer;
  auto frame = [&]() {
    artboard->advance(0.0f);
    artboard->draw(&renderer);
  };
  frame();

  REQUIRE(text->runs().size() == 1);
  TextStylePaint* paint = text->runs().front()->style();
  REQUIRE(paint != nullptr);
  REQUIRE(paint->parent() != nullptr);
  REQUIRE(!paint->parent()->is<Text>());
  REQUIRE(paint->owner() != nullptr);
  REQUIRE(paint->decorations().size() == 1);
  TextStyleDecoration* underline = paint->decorations().front();
  auto bounds = [&]() { return underline->localPath()->rawPath()->bounds(); };
  REQUIRE(!underline->localPath()->empty());
  CHECK(bounds().height() == Approx(2.0f));
  const float top = bounds().minY;

  // Its dashed stroke dashes the current path, not one it cached.
  Stroke* stroke = nullptr;
  for (ShapePaint* shapePaint : underline->shapePaints())
  {
    if (shapePaint->is<Stroke>())
    {
      stroke = shapePaint->as<Stroke>();
    }
  }
  REQUIRE(stroke != nullptr);
  DashPath* dash = nullptr;
  for (Component* child : stroke->children())
  {
    if (child->is<DashPath>())
    {
      dash = child->as<DashPath>();
    }
  }
  REQUIRE(dash != nullptr);
  auto dashed = [&]() { return dash->effectPath(stroke)->rawPath()->bounds(); };
  CHECK(dashed().minY == Approx(bounds().minY));

  underline->thickness(5.0f);
  frame();
  CHECK(bounds().height() == Approx(5.0f));
  CHECK(bounds().minY == Approx(top));
  CHECK(dashed().maxY == Approx(bounds().maxY));

  underline->offset(3.0f);
  frame();
  CHECK(bounds().minY == Approx(top + 3.0f));
  CHECK(dashed().minY == Approx(bounds().minY));

  underline->decorationTypeValue((uint32_t)TextDecorationType::strikethrough);
  frame();
  CHECK(bounds().minY < top);
  CHECK(dashed().minY == Approx(bounds().minY));

  // An opacity-only change reaches the global decoration too.
  REQUIRE(!underline->shapePaints().empty());
  text->opacity(0.5f);
  frame();
  CHECK(underline->shapePaints().front()->renderOpacity() == Approx(0.5f));
}
