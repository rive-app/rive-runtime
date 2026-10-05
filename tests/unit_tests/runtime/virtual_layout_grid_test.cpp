#include "rive/constraints/scrolling/virtual_layout.hpp"
#include <catch.hpp>
#include <vector>

// Grid mode of VirtualLayout: rows of cells on layout's grid lines. Track
// sizing itself is layout's (see yoga_grid_virtual_contributions_test.cpp).
// Mirrors virtual_layout_grid_test.dart in rive_core.

using namespace rive;

namespace
{
struct GridOptions
{
    int columns = 4;
    std::vector<float> columnStarts = {0, 110, 220, 330};
    // Layout's row lines; empty until it has laid the grid out.
    std::vector<float> rowStarts;
    int rowCount = 0;
    LayoutCrossAlign align = LayoutCrossAlign::start;
};

void grid(VirtualLayout& layout,
          const std::vector<float>& heights,
          const GridOptions& options = {})
{
    layout.beginGrid(10, options.columns, options.align, 10, options.rowCount);
    layout.gridColumnStarts() = options.columnStarts;
    layout.gridRowStarts() = options.rowStarts;
    layout.beginSegment();
    for (float height : heights)
    {
        layout.addItem(height, 100);
    }
    layout.end();
}
} // namespace

TEST_CASE("VirtualLayout grid rows hold one item per column",
          "[virtual_layout]")
{
    VirtualLayout layout;
    grid(layout, std::vector<float>(10, 60));
    CHECK(layout.isGrid());
    CHECK(layout.lineCount() == 3);
    CHECK(layout.lineLastItem(0) == 3);
    CHECK(layout.lineFirstItem(2) == 8);
    CHECK(layout.lineStart(1) == 70);
    CHECK(layout.lineStart(2) == 140);
    CHECK(layout.extent() == 200);
}

TEST_CASE("VirtualLayout grid items take their column start",
          "[virtual_layout]")
{
    VirtualLayout layout;
    grid(layout, std::vector<float>(6, 60));
    float expected[] = {0, 110, 220, 330, 0, 110};
    for (int i = 0; i < 6; i++)
    {
        CHECK(layout.itemFlowOffset(i) == expected[i]);
    }
}

TEST_CASE("VirtualLayout grid rows", "[virtual_layout]")
{
    VirtualLayout layout;
    SECTION("sit on layout's lines once it has them")
    {
        // Lines from a padded grid: rows 80, 60 and 50 tall, gaps of 10.
        GridOptions options;
        options.rowStarts = {5, 95, 165, 215};
        grid(layout, std::vector<float>(12, 30), options);
        CHECK(layout.lineStart(0) == 0);
        CHECK(layout.lineStart(1) == 90);
        CHECK(layout.lineExtent(1) == 60);
        CHECK(layout.lineStart(2) == 160);
        CHECK(layout.lineExtent(2) == 50);
        CHECK(layout.extent() == 210);
    }
    SECTION("stack at their tallest item until then")
    {
        grid(layout, {60, 90, 60, 60, 30});
        CHECK(layout.lineStart(1) == 100);
        CHECK(layout.extent() == 130);
    }
    SECTION("past the lines layout has stack on after them")
    {
        GridOptions options;
        options.rowStarts = {0, 80};
        grid(layout, std::vector<float>(12, 30), options);
        CHECK(layout.lineExtent(0) == 80);
        CHECK(layout.lineStart(1) == 90);
        CHECK(layout.lineExtent(1) == 30);
        CHECK(layout.lineStart(2) == 130);
    }
    SECTION("template rows nothing fills still take their space")
    {
        GridOptions options;
        options.rowCount = 4;
        options.rowStarts = {0, 30, 70, 110, 150};
        grid(layout, std::vector<float>(4, 30), options);
        CHECK(layout.lineCount() == 4);
        CHECK(layout.lineFirstItem(3) == 4);
        CHECK(layout.extent() == 150);
    }
}

TEST_CASE("VirtualLayout grid reports each row's and column's largest item",
          "[virtual_layout]")
{
    VirtualLayout layout;
    layout.beginGrid(10, 2, LayoutCrossAlign::start, 10, 3);
    layout.beginSegment();
    layout.addItem(20, 50);
    layout.addItem(40, 70);
    layout.addItem(30, 90);
    layout.end();
    std::vector<float> rows = {40, 30, 0};
    std::vector<float> columns = {90, 70};
    CHECK(layout.gridRowContents() == rows);
    CHECK(layout.gridColumnContents() == columns);
}

TEST_CASE("VirtualLayout grid only centers within a row", "[virtual_layout]")
{
    VirtualLayout layout;
    GridOptions options;
    options.align = LayoutCrossAlign::center;
    grid(layout, {60, 20}, options);
    CHECK(layout.itemLineOffset(1) == 20);
    // Yoga's grid reads the FlexEnd that end alignment writes as start.
    options.align = LayoutCrossAlign::end;
    grid(layout, {60, 20}, options);
    CHECK(layout.itemLineOffset(1) == 0);
}

TEST_CASE("VirtualLayout grid fallbacks", "[virtual_layout]")
{
    VirtualLayout layout;
    SECTION("missing column starts place items at 0")
    {
        GridOptions options;
        options.columnStarts = {};
        grid(layout, std::vector<float>(4, 60), options);
        CHECK(layout.itemFlowOffset(3) == 0);
    }
    SECTION("no template columns means one column")
    {
        GridOptions options;
        options.columns = 0;
        grid(layout, std::vector<float>(3, 60), options);
        CHECK(layout.lineCount() == 3);
    }
}

TEST_CASE("VirtualLayout grid windows select whole rows", "[virtual_layout]")
{
    VirtualLayout layout;
    grid(layout, std::vector<float>(20, 60));
    auto window = layout.window(65, 130, false, 0);
    CHECK(window.start == 1);
    CHECK(window.end == 2);
    CHECK(layout.lineFirstItem(window.start) == 4);
    CHECK(layout.lineLastItem(window.end) == 11);
}

namespace
{
// Four 100px columns with 10px gaps: lines at 0, 110, 220, 330, 430.
void columns(VirtualLayout& layout, bool withLines = true)
{
    layout.beginGrid(10, 4, LayoutCrossAlign::start, 10);
    if (withLines)
    {
        layout.gridColumnStarts() = {0, 110, 220, 330, 430};
    }
    layout.beginSegment();
    layout.addItem(60, 100);
    layout.end();
}
} // namespace

TEST_CASE("VirtualLayout grid column windows", "[virtual_layout]")
{
    VirtualLayout layout;
    columns(layout);
    SECTION("select the columns a viewport reaches")
    {
        auto window = layout.columnWindow(0, 200, 0);
        // Column 2 starts at 220, past the far edge.
        CHECK(window.start == 0);
        CHECK(window.end == 1);
        auto scrolled = layout.columnWindow(150, 200, 0);
        CHECK(scrolled.start == 1);
        CHECK(scrolled.end == 3);
    }
    SECTION("a column ending at the offset is off screen")
    {
        CHECK(layout.columnWindow(100, 200, 0).visibleStart == 1);
    }
    SECTION("buffer columns either side, clamped to the grid")
    {
        auto window = layout.columnWindow(150, 100, 1);
        CHECK(window.visibleStart == 1);
        CHECK(window.visibleEnd == 2);
        CHECK(window.start == 0);
        CHECK(window.end == 3);
    }
    SECTION("without resolved lines every column is realized")
    {
        columns(layout, false);
        auto window = layout.columnWindow(150, 100, 0);
        CHECK(window.start == 0);
        CHECK(window.end == 3);
    }
}

TEST_CASE("VirtualLayout grid columns cycle", "[virtual_layout]")
{
    VirtualLayout layout;
    columns(layout);
    SECTION("a cycle is the columns plus one gap")
    {
        CHECK(layout.columnCycleExtent() == Approx(440.0f));
        CHECK(layout.columnStart(4) == Approx(440.0f));
        CHECK(layout.columnStart(-1) == Approx(-110.0f));
        CHECK(layout.wrapColumn(-1) == 3);
        CHECK(layout.wrapColumn(5) == 1);
        columns(layout, false);
        CHECK(layout.columnCycleExtent() == 0.0f);
    }
    SECTION("windows run on into the next cycle")
    {
        auto window = layout.columnWindow(300, 200, 0, true);
        CHECK(window.visibleStart == 2);
        CHECK(window.visibleEnd == 4);
    }
    SECTION("and back into the previous one")
    {
        auto window = layout.columnWindow(-50, 100, 0, true);
        CHECK(window.visibleStart == -1);
        CHECK(window.visibleEnd == 0);
    }
    SECTION("the trailing gap leads to the next cycle's first column")
    {
        auto window = layout.columnWindow(435, 50, 0, true);
        CHECK(window.visibleStart == 4);
        CHECK(window.visibleEnd == 4);
    }
    SECTION("buffer and viewport stay within one cycle")
    {
        auto buffered = layout.columnWindow(300, 200, 1, true);
        CHECK(buffered.start == 1);
        CHECK(buffered.end == 4);
        auto wide = layout.columnWindow(0, 2000, 2, true);
        CHECK(wide.start == 0);
        CHECK(wide.end == 3);
    }
}

TEST_CASE("VirtualLayout cycling column windows match a brute force sweep",
          "[virtual_layout]")
{
    VirtualLayout layout;
    columns(layout);
    const float starts[] = {0, 110, 220, 330};
    auto startOf = [&](int column) {
        int wrapped = ((column % 4) + 4) % 4;
        return (float)((column - wrapped) / 4) * 440.0f + starts[wrapped];
    };
    for (float viewport : {0.0f, 50.0f, 100.0f, 200.0f, 440.0f, 1000.0f})
    {
        for (float offset = -1000.0f; offset <= 1000.0f; offset += 0.25f)
        {
            // First column ending past the offset, through the last starting
            // before the far edge, at most one cycle.
            int start = -20;
            while (startOf(start) + 100.0f <= offset)
            {
                start++;
            }
            int end = start;
            while (end < start + 3 && startOf(end + 1) < offset + viewport)
            {
                end++;
            }
            auto window = layout.columnWindow(offset, viewport, 0, true);
            INFO("offset " << offset << " viewport " << viewport);
            REQUIRE(window.visibleStart == start);
            REQUIRE(window.visibleEnd == end);
        }
    }
}
