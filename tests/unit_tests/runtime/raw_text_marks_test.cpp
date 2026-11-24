#include "catch.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/no_op_renderer.hpp"
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

// Keeps the bounds and contour count of what it was given.
class BoundsPath : public RenderPath
{
public:
  AABB bounds = AABB::forExpansion();
  int contours = 0;

  void rewind() override
  {
    bounds = AABB::forExpansion();
    contours = 0;
  }
  void fillRule(FillRule) override {}
  void addPath(CommandPath*, const Mat2D&) override {}
  void addRenderPath(const RenderPath*, const Mat2D&) override {}
  void moveTo(float, float) override {}
  void lineTo(float, float) override {}
  void cubicTo(float, float, float, float, float, float) override {}
  void close() override {}
  void addRawPath(const RawPath& path) override
  {
    bounds.expand(path.bounds());
    for (PathVerb verb : path.verbs())
    {
      if (verb == PathVerb::move)
      {
        contours++;
      }
    }
  }
};

class BoundsFactory : public NoOpFactory
{
public:
  rcp<RenderPath> makeEmptyRenderPath() override
  {
    return make_rcp<BoundsPath>();
  }
  rcp<RenderPaint> paint()
  {
    return static_cast<Factory*>(this)->makeRenderPaint();
  }
};

struct Draw
{
  BoundsPath* path;
  RenderPaint* paint;
};

class DrawRecorder : public NoOpRenderer
{
public:
  std::vector<Draw> draws;
  void drawPath(RenderPath* path, RenderPaint* paint) override
  {
    draws.push_back({static_cast<BoundsPath*>(path), paint});
  }
  // The draws made with paint, in order.
  std::vector<size_t> indicesOf(RenderPaint* paint) const
  {
    std::vector<size_t> out;
    for (size_t i = 0; i < draws.size(); i++)
    {
      if (draws[i].paint == paint)
      {
        out.push_back(i);
      }
    }
    return out;
  }
};

const char* kLatin = "assets/fonts/Inter_18pt-Regular.ttf";

struct MarkedText
{
  BoundsFactory factory;
  RawText text{&factory};
  rcp<Font> font = loadFont(kLatin);
  rcp<RenderPaint> ink = factory.paint();
  rcp<RenderPaint> yellow = factory.paint();
  rcp<RenderPaint> blue = factory.paint();
  rcp<RenderPaint> red = factory.paint();

  RawText::RunMarks marks()
  {
    RawText::RunMarks marks;
    marks.highlight = yellow;
    marks.underline = blue;
    marks.strikethrough = red;
    return marks;
  }
};
} // namespace

TEST_CASE("run marks draw around the glyphs in CSS order", "[rawtext]")
{
  MarkedText marked;
  marked.text.append("Hello ", marked.ink, marked.font, 20.0f);
  marked.text.append(
      "world",
      marked.ink,
      marked.font,
      20.0f,
      -1,
      0,
      0xFF000000,
      marked.marks());

  DrawRecorder recorder;
  marked.text.render(&recorder);
  auto highlight = recorder.indicesOf(marked.yellow.get());
  auto underline = recorder.indicesOf(marked.blue.get());
  auto glyphs = recorder.indicesOf(marked.ink.get());
  auto strike = recorder.indicesOf(marked.red.get());
  REQUIRE(highlight.size() == 1);
  REQUIRE(underline.size() == 1);
  REQUIRE(strike.size() == 1);
  REQUIRE(!glyphs.empty());
  CHECK(highlight[0] < underline[0]);
  CHECK(underline[0] < glyphs.front());
  CHECK(glyphs.back() < strike[0]);
}

TEST_CASE("run marks sit where the font puts them", "[rawtext]")
{
  MarkedText marked;
  marked.text.append("Hello ", marked.ink, marked.font, 20.0f);
  marked.text.append(
      "world",
      marked.ink,
      marked.font,
      20.0f,
      -1,
      0,
      0xFF000000,
      marked.marks());
  REQUIRE(marked.text.lineCount() == 1);
  const OrderedLine& line = marked.text.orderedLines()[0];
  const Font& font = *marked.font;

  // Where "world" starts and the line ends.
  float worldStart = 0.0f, lineEnd = line.startX();
  marked.text.forEachGlyph(0, [&](const GlyphRun& run, uint32_t i, Vec2D at) {
    if (run.textIndices[i] == 6)
    {
      worldStart = at.x - run.offsets[i].x;
    }
    lineEnd += marked.text.glyphAdvance(0, run, i);
    return true;
  });

  DrawRecorder recorder;
  marked.text.render(&recorder);
  auto boundsOf = [&](RenderPaint* paint) {
    auto indices = recorder.indicesOf(paint);
    REQUIRE(indices.size() == 1);
    return recorder.draws[indices[0]].path->bounds;
  };

  AABB highlight = boundsOf(marked.yellow.get());
  CHECK(highlight.minX == Approx(worldStart));
  CHECK(highlight.maxX == Approx(lineEnd));
  CHECK(highlight.minY == Approx(line.top()));
  CHECK(highlight.maxY == Approx(line.bottom()));

  AABB underline = boundsOf(marked.blue.get());
  CHECK(underline.minX == Approx(worldStart));
  CHECK(underline.minY == Approx(line.y() + font.underlinePosition(20.0f)));
  CHECK(underline.height() == Approx(font.underlineThickness(20.0f)));

  AABB strike = boundsOf(marked.red.get());
  CHECK(strike.minY == Approx(line.y() + font.strikeoutPosition(20.0f)));
  CHECK(strike.height() == Approx(font.strikeoutThickness(20.0f)));
  // Above the baseline, unlike the underline.
  CHECK(strike.maxY < line.y());
  CHECK(underline.minY > line.y());
}

TEST_CASE("run marks join into one bar per line", "[rawtext]")
{
  MarkedText marked;
  RawText::RunMarks underlineOnly;
  underlineOnly.underline = marked.blue;
  marked.text.append(
      "one two three four five six seven",
      marked.ink,
      marked.font,
      20.0f,
      -1,
      0,
      0xFF000000,
      underlineOnly);
  marked.text.sizing(TextSizing::autoHeight);
  marked.text.maxWidth(120.0f);
  uint32_t lines = marked.text.lineCount();
  REQUIRE(lines >= 2);

  DrawRecorder recorder;
  marked.text.render(&recorder);
  auto underline = recorder.indicesOf(marked.blue.get());
  REQUIRE(underline.size() == 1);
  CHECK(recorder.draws[underline[0]].path->contours == (int)lines);
}

TEST_CASE("a given paint replaces lines but not highlights", "[rawtext]")
{
  MarkedText marked;
  marked.text.append(
      "marked",
      marked.ink,
      marked.font,
      20.0f,
      -1,
      0,
      0xFF000000,
      marked.marks());
  auto shadow = marked.factory.paint();

  DrawRecorder recorder;
  marked.text.render(&recorder, shadow);
  CHECK(recorder.indicesOf(marked.yellow.get()).size() == 1);
  CHECK(recorder.indicesOf(marked.blue.get()).empty());
  CHECK(recorder.indicesOf(marked.red.get()).empty());
  CHECK(recorder.indicesOf(marked.ink.get()).empty());
  // Underline, glyphs and strikethrough.
  CHECK(recorder.indicesOf(shadow.get()).size() == 3);
}

TEST_CASE("a mark joins runs whose glyph paints differ", "[rawtext]")
{
  MarkedText marked;
  RawText::RunMarks blueLine;
  blueLine.underline = marked.blue;
  // Alternating inks under one underline paint: a stroked underline must
  // not get an edge where the ink changes.
  const char* words[] = {"one ", "two ", "three ", "four ", "five ", "six"};
  for (int i = 0; i < 6; i++)
  {
    marked.text.append(
        words[i],
        i % 2 == 0 ? marked.ink : marked.yellow,
        marked.font,
        20.0f,
        -1,
        0,
        0xFF000000,
        blueLine);
  }
  CHECK(marked.text.styleCount() == 2);
  marked.text.sizing(TextSizing::autoHeight);
  marked.text.maxWidth(120.0f);
  const uint32_t lines = marked.text.lineCount();
  REQUIRE(lines >= 3);

  DrawRecorder recorder;
  marked.text.render(&recorder);
  auto underline = recorder.indicesOf(marked.blue.get());
  REQUIRE(underline.size() == 1);
  CHECK(recorder.draws[underline[0]].path->contours == (int)lines);

  // A range of lines joins them the same way.
  recorder.draws.clear();
  marked.text.renderLines(&recorder, 1, 1);
  underline = recorder.indicesOf(marked.blue.get());
  REQUIRE(underline.size() == 1);
  CHECK(recorder.draws[underline[0]].path->contours == 1);
}

TEST_CASE("a range of lines draws only its own marks", "[rawtext]")
{
  MarkedText marked;
  RawText::RunMarks underlineOnly;
  underlineOnly.underline = marked.blue;
  marked.text.append(
      "one two three four five six seven",
      marked.ink,
      marked.font,
      20.0f,
      -1,
      0,
      0xFF000000,
      underlineOnly);
  marked.text.sizing(TextSizing::autoHeight);
  marked.text.maxWidth(120.0f);
  REQUIRE(marked.text.lineCount() >= 3);
  const OrderedLine& second = marked.text.orderedLines()[1];

  DrawRecorder recorder;
  marked.text.renderLines(&recorder, 1, 1);
  auto underline = recorder.indicesOf(marked.blue.get());
  REQUIRE(underline.size() == 1);
  const BoundsPath& path = *recorder.draws[underline[0]].path;
  CHECK(path.contours == 1);
  CHECK(path.bounds.minY >= second.top());
  CHECK(path.bounds.maxY <= second.bottom());
}

TEST_CASE("runs share marks only with the same paints", "[rawtext]")
{
  MarkedText marked;
  RawText::RunMarks blueLine;
  blueLine.underline = marked.blue;
  RawText::RunMarks redLine;
  redLine.underline = marked.red;
  marked.text.append(
      "one ",
      marked.ink,
      marked.font,
      20.0f,
      -1,
      0,
      0xFF000000,
      blueLine);
  marked.text.append(
      "two ",
      marked.ink,
      marked.font,
      20.0f,
      -1,
      0,
      0xFF000000,
      blueLine);
  marked.text.append(
      "three",
      marked.ink,
      marked.font,
      20.0f,
      -1,
      0,
      0xFF000000,
      redLine);
  marked.text.append("four", marked.ink, marked.font, 20.0f);
  // Plain, blue and red: three styles.
  CHECK(marked.text.styleCount() == 3);

  DrawRecorder recorder;
  marked.text.render(&recorder);
  auto blue = recorder.indicesOf(marked.blue.get());
  auto red = recorder.indicesOf(marked.red.get());
  REQUIRE(blue.size() == 1);
  REQUIRE(red.size() == 1);
  // "one two " is one bar.
  CHECK(recorder.draws[blue[0]].path->contours == 1);
  CHECK(
      recorder.draws[blue[0]].path->bounds.maxX <=
      recorder.draws[red[0]].path->bounds.minX + 0.001f);
}

TEST_CASE(
    "a justified line's underline crosses its widened spaces",
    "[rawtext]")
{
  MarkedText marked;
  RawText::RunMarks underlineOnly;
  underlineOnly.underline = marked.blue;
  marked.text.append(
      "one two three four five six seven eight nine",
      marked.ink,
      marked.font,
      20.0f,
      -1,
      0,
      0xFF000000,
      underlineOnly);
  marked.text.sizing(TextSizing::autoHeight);
  marked.text.maxWidth(150.0f);
  marked.text.align(TextAlign::justify);
  REQUIRE(marked.text.lineCount() >= 2);

  DrawRecorder recorder;
  marked.text.renderLines(&recorder, 0, 0);
  auto underline = recorder.indicesOf(marked.blue.get());
  REQUIRE(underline.size() == 1);
  const BoundsPath& path = *recorder.draws[underline[0]].path;
  CHECK(path.contours == 1);
  CHECK(path.bounds.minX == Approx(0.0f).margin(0.001f));
  CHECK(path.bounds.maxX == Approx(150.0f));
}

// Hidden: each new group scans the ones before it, so 65k of them take ~16 s
// in a debug build. Run it by name or with "[rawtext]".
TEST_CASE(
    "marks past a 16-bit count of groups keep their paints",
    "[.][rawtext]")
{
  MarkedText marked;
  // One paint per run for all three marks makes three groups a run: 21,846
  // runs make 65,538, past what a 16-bit group index holds.
  const int runs = 21846;
  std::vector<rcp<RenderPaint>> paints;
  paints.reserve(runs);
  for (int i = 0; i < runs; i++)
  {
    paints.push_back(marked.factory.paint());
    RawText::RunMarks marks;
    marks.highlight = paints.back();
    marks.underline = paints.back();
    marks.strikethrough = paints.back();
    marked.text
        .append("a", marked.ink, marked.font, 20.0f, -1, 0, 0xFF000000, marks);
  }

  DrawRecorder recorder;
  marked.text.render(&recorder);
  // The last runs' groups sit past index 65,535: each paint still draws its
  // highlight, underline and strikethrough.
  for (int i = runs - 2; i < runs; i++)
  {
    CHECK(recorder.indicesOf(paints[i].get()).size() == 3);
  }
  CHECK(recorder.indicesOf(paints[0].get()).size() == 3);
}
