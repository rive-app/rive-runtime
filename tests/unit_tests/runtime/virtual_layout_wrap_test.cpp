#include "rive/constraints/scrolling/virtual_layout.hpp"
#include <catch.hpp>
#include <utility>
#include <vector>

// Wrap mode of VirtualLayout: line breaks, justify and align. Mirrors
// virtual_layout_wrap_test.dart in rive_core.

using namespace rive;

namespace
{
struct WrapOptions
{
    float lineGap = 10;
    float flowGap = 10;
    float flowStart = 0;
    float flowExtent = 500;
    LayoutMainDistribute justify = LayoutMainDistribute::start;
    LayoutCrossAlign align = LayoutCrossAlign::start;
    bool hugsLines = false;
};

// Items as (scroll extent, flow extent) in one segment.
void wrap(VirtualLayout& layout,
          const std::vector<std::pair<float, float>>& items,
          WrapOptions options = {})
{
    layout.beginWrap(options.lineGap,
                     options.flowGap,
                     options.flowStart,
                     options.flowExtent,
                     options.justify,
                     options.align,
                     options.hugsLines);
    layout.beginSegment();
    for (auto& item : items)
    {
        layout.addItem(item.first, item.second);
    }
    layout.end();
}

std::vector<std::pair<float, float>> uniform(int count)
{
    return std::vector<std::pair<float, float>>(count, {60.0f, 100.0f});
}
} // namespace

TEST_CASE("VirtualLayout wrap breaks lines where flex wrap does",
          "[virtual_layout]")
{
    // 4 * 100 + 3 * 10 = 430 fits 500; a fifth needs 540.
    VirtualLayout layout;
    wrap(layout, uniform(20));
    REQUIRE(layout.lineCount() == 5);
    for (int line = 0; line < 5; line++)
    {
        CHECK(layout.lineFirstItem(line) == line * 4);
        CHECK(layout.lineLastItem(line) == line * 4 + 3);
        CHECK(layout.lineStart(line) == line * 70.0f);
    }
    CHECK(layout.extent() == 340);
    CHECK(layout.lineOfItem(9) == 2);
    CHECK(layout.itemFlowOffset(0) == 0);
    CHECK(layout.itemFlowOffset(1) == 110);
    CHECK(layout.itemFlowOffset(3) == 330);
}

TEST_CASE("VirtualLayout wrap line edge cases", "[virtual_layout]")
{
    VirtualLayout layout;
    SECTION("a line that exactly fills the flow extent does not break")
    {
        WrapOptions options;
        options.flowExtent = 430;
        wrap(layout, uniform(8), options);
        CHECK(layout.lineCount() == 2);
        CHECK(layout.lineLastItem(0) == 3);
    }
    SECTION("keeps a partial last line")
    {
        wrap(layout, uniform(10));
        CHECK(layout.lineCount() == 3);
        CHECK(layout.lineFirstItem(2) == 8);
        CHECK(layout.lineLastItem(2) == 9);
    }
    SECTION("an item wider than the line sits alone")
    {
        wrap(layout, {{60, 100}, {60, 600}, {60, 100}});
        CHECK(layout.lineCount() == 3);
        CHECK(layout.lineFirstItem(1) == 1);
        CHECK(layout.lineLastItem(1) == 1);
    }
    SECTION("flow offsets start at flowStart")
    {
        WrapOptions options;
        options.flowStart = 25;
        wrap(layout, uniform(4), options);
        CHECK(layout.itemFlowOffset(0) == 25);
        CHECK(layout.itemFlowOffset(1) == 135);
    }
    SECTION("lines are as thick as their tallest item")
    {
        wrap(layout, {{60, 100}, {90, 100}, {60, 100}, {60, 100}, {40, 100}});
        CHECK(layout.lineStart(1) == 100);
        CHECK(layout.extent() == 140);
    }
}

TEST_CASE("VirtualLayout wrap justifies each line on its own",
          "[virtual_layout]")
{
    // Line 0 has 4 items (70 free), line 1 has 2 (290 free).
    VirtualLayout layout;
    WrapOptions options;
    options.justify = LayoutMainDistribute::center;
    wrap(layout, uniform(6), options);
    CHECK(layout.itemFlowOffset(0) == 35);
    CHECK(layout.itemFlowOffset(4) == 145);

    options.justify = LayoutMainDistribute::end;
    wrap(layout, uniform(6), options);
    CHECK(layout.itemFlowOffset(0) == 70);
    CHECK(layout.itemFlowOffset(5) == 400);

    options.justify = LayoutMainDistribute::spaceBetween;
    wrap(layout, uniform(6), options);
    CHECK(layout.itemFlowOffset(0) == 0);
    CHECK(layout.itemFlowOffset(3) == Approx(400));
    CHECK(layout.itemFlowOffset(5) == Approx(400));

    // Space-between leaves a lone item at the start.
    wrap(layout, uniform(1), options);
    CHECK(layout.itemFlowOffset(0) == 0);
}

TEST_CASE("VirtualLayout wrap aligns items within their line",
          "[virtual_layout]")
{
    VirtualLayout layout;
    WrapOptions options;
    std::vector<std::pair<float, float>> items = {{60, 100}, {20, 100}};
    options.align = LayoutCrossAlign::start;
    wrap(layout, items, options);
    CHECK(layout.itemLineOffset(1) == 0);
    options.align = LayoutCrossAlign::center;
    wrap(layout, items, options);
    CHECK(layout.itemLineOffset(1) == 20);
    options.align = LayoutCrossAlign::end;
    wrap(layout, items, options);
    CHECK(layout.itemLineOffset(1) == 40);
    // The tallest item sets the line, so it never moves.
    CHECK(layout.itemLineOffset(0) == 0);
}

TEST_CASE("VirtualLayout wrap windows select whole lines", "[virtual_layout]")
{
    VirtualLayout layout;
    wrap(layout, uniform(20));
    // Lines 0 [0,60] and 1 [70,130] are on screen; line 2 starts at 140.
    auto window = layout.window(0, 130, false, 0);
    CHECK(window.start == 0);
    CHECK(window.end == 1);
    CHECK(layout.lineLastItem(window.end) == 7);
    auto scrolled = layout.window(65, 130, false, 0);
    CHECK(scrolled.start == 1);
    CHECK(scrolled.end == 2);
}

TEST_CASE("VirtualLayout wrap keeps segments across lines", "[virtual_layout]")
{
    VirtualLayout layout;
    layout.beginWrap(10,
                     10,
                     0,
                     500,
                     LayoutMainDistribute::start,
                     LayoutCrossAlign::start);
    layout.beginSegment();
    layout.addItem(60, 100);
    layout.beginSegment();
    for (int i = 0; i < 6; i++)
    {
        layout.addItem(60, 100);
    }
    layout.end();
    CHECK(layout.segmentOf(0) == 0);
    CHECK(layout.segmentOf(4) == 1);
    CHECK(layout.segmentStart(1) == 1);
    CHECK(layout.lineCount() == 2);
}

TEST_CASE("VirtualLayout linear reports no flow or line offsets",
          "[virtual_layout]")
{
    VirtualLayout layout;
    layout.buildLinear(3, [](int) { return 50.0f; }, 10);
    CHECK_FALSE(layout.wraps());
    CHECK(layout.itemFlowOffset(1) == 0);
    CHECK(layout.itemLineOffset(1) == 0);
}

TEST_CASE("VirtualLayout wrap hugging lines justify within the widest",
          "[virtual_layout]")
{
    // Lines still break at 500; the widest uses 430, so a 2 item line (210)
    // centers in 430, not 500.
    VirtualLayout layout;
    WrapOptions options;
    options.justify = LayoutMainDistribute::center;
    options.hugsLines = true;
    wrap(layout, uniform(6), options);
    CHECK(layout.lineCount() == 2);
    CHECK(layout.itemFlowOffset(0) == 0);
    CHECK(layout.itemFlowOffset(4) == 110);
}
