#include "catch.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/no_op_renderer.hpp"
#include <cstdio>
#include <set>
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

// A render path that keeps the bounds of what it was given.
class BoundsPath : public RenderPath
{
public:
  AABB bounds = AABB::forExpansion();

  void rewind() override { bounds = AABB::forExpansion(); }
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
  }
};

class BoundsFactory : public NoOpFactory
{
public:
  rcp<RenderPath> makeEmptyRenderPath() override
  {
    return make_rcp<BoundsPath>();
  }
  // NoOpFactory keeps its overrides private; Factory's are public.
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

  AABB bounds() const
  {
    AABB out = AABB::forExpansion();
    for (const Draw& draw : draws)
    {
      out.expand(draw.path->bounds);
    }
    return out;
  }

  std::set<RenderPath*> paths() const
  {
    std::set<RenderPath*> out;
    for (const Draw& draw : draws)
    {
      out.insert(draw.path);
    }
    return out;
  }
};

// Top and bottom of a line in the text's local space.
std::pair<float, float> lineExtent(RawText& text, uint32_t line)
{
  const OrderedLine& orderedLine = text.orderedLines()[line];
  const GlyphLine& glyphLine = orderedLine.glyphLine();
  float baseline = orderedLine.y();
  return {
      baseline - glyphLine.baseline + glyphLine.top,
      baseline - glyphLine.baseline + glyphLine.bottom};
}

// Wraps to several lines in two paints: "red" on the first few words and
// "blue" on the rest.
struct WrappedText
{
  BoundsFactory factory;
  RawText text{&factory};
  rcp<RenderPaint> red = factory.paint();
  rcp<RenderPaint> blue = factory.paint();

  WrappedText()
  {
    auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
    text.append("one two three ", red, font, 20.0f);
    text.append("four five six seven eight nine", blue, font, 20.0f);
    text.sizing(TextSizing::autoHeight);
    text.maxWidth(70.0f);
    REQUIRE(text.lineCount() >= 4);
  }
};
} // namespace

TEST_CASE("RawText draws a range of lines where they sit", "[rawtext]")
{
  WrappedText wrapped;
  RawText& text = wrapped.text;

  for (uint32_t line = 0; line < text.lineCount(); line++)
  {
    DrawRecorder recorder;
    text.renderLines(&recorder, line, line);
    REQUIRE(!recorder.draws.empty());
    auto [top, bottom] = lineExtent(text, line);
    AABB drawn = recorder.bounds();
    // Only this line's glyphs: nothing reaches into its neighbors.
    CHECK(drawn.minY >= top - 0.5f);
    CHECK(drawn.maxY <= bottom + 0.5f);
    CHECK(drawn.maxY > top);
  }

  // Two lines span both, and no other.
  DrawRecorder recorder;
  text.renderLines(&recorder, 1, 2);
  AABB drawn = recorder.bounds();
  CHECK(drawn.minY >= lineExtent(text, 1).first - 0.5f);
  CHECK(drawn.maxY <= lineExtent(text, 2).second + 0.5f);
  CHECK(drawn.maxY > lineExtent(text, 2).first);
}

TEST_CASE("RawText draws every line from the whole text's paths", "[rawtext]")
{
  WrappedText wrapped;
  RawText& text = wrapped.text;

  DrawRecorder whole;
  text.render(&whole);
  DrawRecorder all;
  text.renderLines(&all, 0, text.lineCount() - 1);
  CHECK(all.paths() == whole.paths());

  // Past the end clamps to the last line.
  DrawRecorder past;
  text.renderLines(&past, 0, 1000);
  CHECK(past.paths() == whole.paths());
}

TEST_CASE(
    "RawText clamps line ranges and draws nothing for empty ones",
    "[rawtext]")
{
  WrappedText wrapped;
  RawText& text = wrapped.text;
  uint32_t last = text.lineCount() - 1;

  DrawRecorder tail;
  text.renderLines(&tail, last - 1, last);
  DrawRecorder clamped;
  text.renderLines(&clamped, last - 1, 1000);
  CHECK(clamped.paths() == tail.paths());

  DrawRecorder reversed;
  text.renderLines(&reversed, 2, 1);
  CHECK(reversed.draws.empty());

  DrawRecorder pastEnd;
  text.renderLines(&pastEnd, last + 1, last + 5);
  CHECK(pastEnd.draws.empty());

  BoundsFactory factory;
  RawText empty(&factory);
  DrawRecorder none;
  empty.renderLines(&none, 0, 3);
  CHECK(none.draws.empty());
}

TEST_CASE("RawText keeps a range's paths until the text changes", "[rawtext]")
{
  WrappedText wrapped;
  RawText& text = wrapped.text;

  DrawRecorder first;
  text.renderLines(&first, 1, 2);
  DrawRecorder again;
  text.renderLines(&again, 1, 2);
  // The same paths, not rebuilt.
  CHECK(again.paths() == first.paths());

  // Drawing more ranges than it keeps still draws each one right.
  for (uint32_t pass = 0; pass < 2; pass++)
  {
    for (uint32_t line = 0; line < text.lineCount(); line++)
    {
      DrawRecorder recorder;
      text.renderLines(&recorder, line, line);
      auto [top, bottom] = lineExtent(text, line);
      CHECK(recorder.bounds().minY >= top - 0.5f);
      CHECK(recorder.bounds().maxY <= bottom + 0.5f);
    }
  }

  // A change lays the text out again: the range follows the new layout.
  text.maxWidth(1000.0f);
  REQUIRE(text.lineCount() == 1);
  DrawRecorder single;
  text.renderLines(&single, 0, 0);
  DrawRecorder whole;
  text.render(&whole);
  CHECK(single.paths() == whole.paths());
  DrawRecorder gone;
  text.renderLines(&gone, 1, 2);
  CHECK(gone.draws.empty());
}

TEST_CASE(
    "RawText draws a range with the runs' paints or the given one",
    "[rawtext]")
{
  WrappedText wrapped;
  RawText& text = wrapped.text;
  uint32_t last = text.lineCount() - 1;

  // The last line only holds blue words.
  DrawRecorder tail;
  text.renderLines(&tail, last, last);
  REQUIRE(tail.draws.size() == 1);
  CHECK(tail.draws[0].paint == wrapped.blue.get());

  // Every line: both paints, in the order the runs first drew.
  DrawRecorder all;
  text.renderLines(&all, 0, 1);
  std::set<RenderPaint*> paints;
  for (const Draw& draw : all.draws)
  {
    paints.insert(draw.paint);
  }
  CHECK(paints.count(wrapped.red.get()) == 1);

  // A given paint replaces them all.
  auto override = wrapped.factory.paint();
  DrawRecorder overridden;
  text.renderLines(&overridden, 0, last - 1, override);
  REQUIRE(!overridden.draws.empty());
  for (const Draw& draw : overridden.draws)
  {
    CHECK(draw.paint == override.get());
  }
}
