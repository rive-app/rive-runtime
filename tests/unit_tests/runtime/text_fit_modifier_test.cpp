#include "rive/text/text.hpp"
#include "rive/text/text_modifier_group.hpp"
#include "rive/text/text_modifier_range.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <algorithm>
#include <vector>

// From text_fit_modifier_coverage_test.dart, which asserts the same sizes.
static const char* kFixture = "assets/text_fit_modifier_coverage.riv";

namespace
{
struct Extent
{
  float size;
  float width;
  float height;
};

// First run's size, widest line and stacked height.
Extent extentOf(rive::Text* text)
{
  Extent out = {0.0f, 0.0f, 0.0f};
  const rive::Paragraph& paragraph = text->shape()[0];
  out.size = paragraph.runs[0].size;
  for (const rive::OrderedLine& ordered : text->orderedLines())
  {
    const rive::GlyphLine& line = ordered.glyphLine();
    out.width = std::max(
        out.width,
        paragraph.runs[line.endRunIndex].xpos[line.endGlyphIndex] -
            paragraph.runs[line.startRunIndex].xpos[line.startGlyphIndex]);
    out.height = line.bottom;
  }
  return out;
}

const std::vector<float>& coverageOf(rive::Text* text)
{
  return text->modifierGroups()[0]->coverageValues();
}

void fitWWWW(rive::Text* text, rive::TextValueRun* run)
{
  text->overflow(rive::TextOverflow::fitFontSize);
  text->wrapValue((uint8_t)rive::TextWrap::noWrap);
  run->text("WWWW");
}
} // namespace

TEST_CASE(
    "fitFontSize fits a new shape-modified text with its coverage",
    "[text]")
{
  auto file = ReadRiveFile(kFixture);
  auto artboard = file->artboard("My Artboard");
  auto text = artboard->find<rive::Text>()[0];
  fitWWWW(text, artboard->find<rive::TextValueRun>()[0]);
  artboard->advance(0.0f);
  auto extent = extentOf(text);
  CHECK(extent.size == 79.0f);
  CHECK(extent.width <= 300.0f);
}

// The fit measures the new text with its own coverage, not the old text's.
TEST_CASE("fitFontSize refits a shape-modified text that got shorter", "[text]")
{
  auto file = ReadRiveFile(kFixture);
  auto artboard = file->artboard("My Artboard");
  auto text = artboard->find<rive::Text>()[0];
  artboard->advance(0.0f);
  fitWWWW(text, artboard->find<rive::TextValueRun>()[0]);
  artboard->advance(0.0f);
  auto extent = extentOf(text);
  CHECK(extent.size == 79.0f);
  CHECK(extent.width <= 300.0f);
}

TEST_CASE("fitFontSize measures a line range at each probed size", "[text]")
{
  auto file = ReadRiveFile(kFixture);
  auto artboard = file->artboard("lines");
  auto text = artboard->find<rive::Text>()[0];
  artboard->advance(0.0f);
  auto extent = extentOf(text);
  CHECK(extent.size == 99.0f);
  CHECK(extent.width <= 400.0f);
  CHECK(extent.height <= 400.0f);
}

// Line ranges make the fit non-monotonic, so the probe order matters.
TEST_CASE("fitFontSize tries sizes in the editor's order", "[text]")
{
  auto file = ReadRiveFile(kFixture);
  // The authored size fits at 220 wide; at 200 the search runs below it.
  auto authored = file->artboard("authored");
  authored->advance(0.0f);
  CHECK(extentOf(authored->find<rive::Text>()[0]).size == 84.0f);
  auto searched = file->artboard("searched");
  searched->advance(0.0f);
  CHECK(extentOf(searched->find<rive::Text>()[0]).size == 74.0f);
}

TEST_CASE("fitFontSize measure leaves coverage at the fitted size", "[text]")
{
  auto file = ReadRiveFile(kFixture);
  auto artboard = file->artboard("lines");
  auto text = artboard->find<rive::Text>()[0];
  // A box where the search's last probe isn't the size it settles on.
  text->width(300.0f);
  text->height(420.0f);
  text->measureLayout(
      300.0f,
      rive::LayoutMeasureMode::exactly,
      420.0f,
      rive::LayoutMeasureMode::atMost);
  REQUIRE(text->shapeCount(rive::Text::ShapeSource::fit) > 0);
  auto measured = coverageOf(text);
  // The update fits the same box and recomputes coverage from its shape.
  artboard->advance(0.0f);
  CHECK(measured == coverageOf(text));
}

TEST_CASE(
    "fitFontSize update ignores the range maps of a measure probe",
    "[text]")
{
  auto file = ReadRiveFile(kFixture);
  auto measured = file->artboardNamed("lines");
  auto untouched = file->artboardNamed("lines");
  auto text = measured->find<rive::Text>()[0];
  text->measureLayout(
      200.0f,
      rive::LayoutMeasureMode::exactly,
      400.0f,
      rive::LayoutMeasureMode::atMost);
  measured->advance(0.0f);
  untouched->advance(0.0f);
  CHECK(coverageOf(text) == coverageOf(untouched->find<rive::Text>()[0]));
}

// Without a shape modifier no reshape is pending to rebuild cleared maps.
TEST_CASE("fitFontSize measure leaves an opacity-only group alone", "[text]")
{
  auto file = ReadRiveFile(kFixture);
  auto artboard = file->artboard("opacity");
  auto text = artboard->find<rive::Text>()[0];
  artboard->advance(0.0f);
  text->modifierGroups()[0]->ranges()[0]->modifyTo(0.6f);
  text->measureLayout(
      300.0f,
      rive::LayoutMeasureMode::exactly,
      61.0f,
      rive::LayoutMeasureMode::atMost);
  artboard->advance(0.0f);
  const auto& coverage = coverageOf(text);
  REQUIRE(coverage.size() == 5);
  CHECK(coverage[0] == 1.0f);
  CHECK(coverage[1] == 1.0f);
  CHECK(coverage[2] == 1.0f);
}
