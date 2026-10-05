#include "rive/constraints/scrolling/virtual_layout.hpp"
#include <catch.hpp>
#include <vector>

// Grid mode of VirtualLayout: rows of cells, column starts and row tracks.
// Mirrors virtual_layout_grid_test.dart in rive_core.

using namespace rive;

namespace
{
struct GridOptions
{
    int columns = 4;
    std::vector<float> columnStarts = {0, 110, 220, 330};
    std::vector<VirtualGridTrack> templateRows;
    std::vector<VirtualGridTrack> autoRows;
    float rowSpace = -1.0f;
    LayoutCrossAlign align = LayoutCrossAlign::start;
};

void grid(VirtualLayout& layout,
          const std::vector<float>& heights,
          const GridOptions& options = {})
{
    layout.beginGrid(10, options.columns, options.align);
    layout.gridColumnStarts() = options.columnStarts;
    layout.gridRows().templates = options.templateRows;
    layout.gridRows().autos = options.autoRows;
    layout.gridRows().space = options.rowSpace;
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

TEST_CASE("VirtualLayout grid row tracks", "[virtual_layout]")
{
    VirtualLayout layout;
    SECTION("fixed rows hold their size and auto rows cycle")
    {
        // Template rows 80 then content-sized; auto rows cycle 40, 50.
        GridOptions options;
        options.templateRows = {VirtualGridTrack::points(80),
                                VirtualGridTrack::autoSize()};
        options.autoRows = {VirtualGridTrack::points(40),
                            VirtualGridTrack::points(50)};
        grid(layout, std::vector<float>(20, 60), options);
        float expected[] = {0, 90, 160, 210, 270};
        for (int r = 0; r < 5; r++)
        {
            CHECK(layout.lineStart(r) == expected[r]);
        }
        CHECK(layout.extent() == 310);
    }
    SECTION("a content sized row fits its tallest item")
    {
        grid(layout, {60, 90, 60, 60, 30});
        CHECK(layout.lineStart(1) == 100);
        CHECK(layout.extent() == 130);
    }
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

namespace
{
// A one column grid's rows as layout sizes them, recorded from the layout
// engine by virtual_layout_grid_rows_test.dart: row tracks, item heights, row
// gap, the grid's inner height (-1 when it hugs its rows), where layout put
// each item and, for a hugging grid, its height.
struct RowCase
{
    std::vector<VirtualGridTrack> rows;
    std::vector<float> heights;
    float gap;
    float rowSpace;
    std::vector<float> tops;
    float height;
};

using S = VirtualTrackSizing;
// clang-format off
const RowCase rowCases[] = {
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::points, 40.0f}, {S::points, 80.0f, S::points, 120.0f}}, {150.0f, 50.0f, 50.0f, 100.0f, 150.0f}, 0.0f, 300.0f, {0.0f, 150.0f, 200.0f, 280.0f, 380.0f}, -1.0f},
    {{}, {150.0f, 200.0f, 80.0f, 80.0f, 100.0f, 150.0f}, 10.0f, -1.0f, {0.0f, 160.0f, 370.0f, 460.0f, 550.0f, 660.0f}, 810.0f},
    {{{S::autoSize, 0.0f, S::fr, 2.0f}, {S::percent, 10.0f, S::percent, 10.0f}, {S::points, 80.0f, S::points, 80.0f}}, {50.0f, 150.0f, 80.0f, 80.0f, 80.0f}, 10.0f, 400.0f, {0.0f, 90.0f, 140.0f, 230.0f, 320.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}}, {150.0f, 80.0f, 50.0f, 80.0f, 80.0f}, 10.0f, 400.0f, {0.0f, 160.0f, 250.0f, 310.0f, 400.0f}, -1.0f},
    {{}, {100.0f, 50.0f, 50.0f, 50.0f, 150.0f}, 0.0f, -1.0f, {0.0f, 100.0f, 150.0f, 200.0f, 250.0f}, 400.0f},
    {{{S::autoSize, 0.0f, S::percent, 10.0f}, {S::autoSize, 0.0f, S::autoSize, 0.0f}}, {20.0f}, 0.0f, -1.0f, {0.0f}, 20.0f},
    {{{S::points, 0.0f, S::points, 0.0f}}, {200.0f, 80.0f, 80.0f, 20.0f, 200.0f}, 0.0f, 300.0f, {0.0f, 0.0f, 80.0f, 160.0f, 180.0f}, -1.0f},
    {{}, {100.0f}, 0.0f, 300.0f, {0.0f}, -1.0f},
    {{{S::points, 40.0f, S::points, 40.0f}}, {20.0f, 20.0f}, 0.0f, 400.0f, {0.0f, 40.0f}, -1.0f},
    {{}, {80.0f, 200.0f, 80.0f, 150.0f}, 0.0f, 300.0f, {0.0f, 80.0f, 280.0f, 360.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}}, {100.0f, 20.0f, 50.0f, 200.0f, 80.0f, 200.0f}, 0.0f, 400.0f, {0.0f, 100.0f, 120.0f, 170.0f, 370.0f, 450.0f}, -1.0f},
    {{{S::points, 0.0f, S::percent, 10.0f}}, {200.0f, 50.0f, 80.0f}, 10.0f, 400.0f, {0.0f, 50.0f, 110.0f}, -1.0f},
    {{}, {50.0f}, 0.0f, 400.0f, {0.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::fr, 0.5f}}, {20.0f, 150.0f, 200.0f, 200.0f}, 10.0f, 400.0f, {0.0f, 30.0f, 190.0f, 400.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::fr, 1.0f}}, {50.0f, 50.0f, 200.0f, 100.0f, 20.0f}, 10.0f, -1.0f, {0.0f, 60.0f, 120.0f, 330.0f, 440.0f}, 460.0f},
    {{{S::autoSize, 0.0f, S::fr, 1.0f}, {S::points, 120.0f, S::points, 80.0f}, {S::autoSize, 0.0f, S::points, 40.0f}}, {200.0f, 100.0f, 80.0f, 20.0f, 200.0f, 200.0f}, 0.0f, 300.0f, {0.0f, 200.0f, 320.0f, 400.0f, 420.0f, 620.0f}, -1.0f},
    {{{S::percent, 25.0f, S::percent, 25.0f}}, {50.0f, 20.0f, 100.0f, 80.0f, 50.0f}, 0.0f, 400.0f, {0.0f, 100.0f, 120.0f, 220.0f, 300.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::points, 80.0f}}, {150.0f, 200.0f, 100.0f, 100.0f, 100.0f}, 0.0f, -1.0f, {0.0f, 150.0f, 350.0f, 450.0f, 550.0f}, 650.0f},
    {{}, {150.0f, 100.0f, 50.0f, 20.0f, 80.0f, 50.0f}, 0.0f, -1.0f, {0.0f, 150.0f, 250.0f, 300.0f, 320.0f, 400.0f}, 450.0f},
    {{{S::autoSize, 0.0f, S::fr, 1.0f}, {S::autoSize, 0.0f, S::points, 80.0f}}, {100.0f, 100.0f}, 10.0f, 400.0f, {0.0f, 300.0f}, -1.0f},
    {{{S::points, 80.0f, S::fr, 2.0f}}, {150.0f, 100.0f, 150.0f, 50.0f, 20.0f}, 0.0f, 400.0f, {0.0f, 80.0f, 180.0f, 330.0f, 380.0f}, -1.0f},
    {{{S::points, 0.0f, S::points, 0.0f}, {S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::percent, 50.0f, S::percent, 50.0f}}, {100.0f}, 10.0f, -1.0f, {0.0f}, 20.0f},
    {{{S::autoSize, 0.0f, S::fr, 1.0f}, {S::autoSize, 0.0f, S::fr, 0.5f}, {S::points, 80.0f, S::points, 0.0f}}, {200.0f, 200.0f, 80.0f, 150.0f, 50.0f, 150.0f}, 0.0f, 300.0f, {0.0f, 200.0f, 400.0f, 480.0f, 630.0f, 680.0f}, -1.0f},
    {{}, {80.0f, 100.0f, 80.0f, 80.0f, 100.0f, 200.0f}, 10.0f, 400.0f, {0.0f, 90.0f, 200.0f, 290.0f, 380.0f, 490.0f}, -1.0f},
    {{}, {80.0f}, 0.0f, 300.0f, {0.0f}, -1.0f},
    {{{S::percent, 10.0f, S::percent, 10.0f}}, {20.0f, 20.0f}, 0.0f, 300.0f, {0.0f, 30.0f}, -1.0f},
    {{{S::percent, 25.0f, S::percent, 50.0f}, {S::points, 0.0f, S::points, 120.0f}, {S::autoSize, 0.0f, S::fr, 0.5f}}, {200.0f, 200.0f, 150.0f, 50.0f, 100.0f}, 0.0f, 300.0f, {0.0f, 75.0f, 75.0f, 225.0f, 275.0f}, -1.0f},
    {{}, {200.0f}, 10.0f, 400.0f, {0.0f}, -1.0f},
    {{{S::points, 120.0f, S::points, 120.0f}}, {20.0f, 50.0f, 20.0f, 20.0f, 50.0f}, 0.0f, -1.0f, {0.0f, 120.0f, 170.0f, 190.0f, 210.0f}, 260.0f},
    {{{S::autoSize, 0.0f, S::points, 80.0f}}, {100.0f, 80.0f, 100.0f}, 0.0f, -1.0f, {0.0f, 100.0f, 180.0f}, 280.0f},
    {{{S::autoSize, 0.0f, S::fr, 1.0f}, {S::percent, 25.0f, S::points, 80.0f}}, {200.0f, 20.0f}, 10.0f, 400.0f, {0.0f, 300.0f}, -1.0f},
    {{}, {50.0f, 20.0f, 200.0f, 20.0f, 200.0f, 20.0f}, 10.0f, -1.0f, {0.0f, 60.0f, 90.0f, 300.0f, 330.0f, 540.0f}, 560.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::fr, 0.5f}}, {150.0f, 50.0f, 200.0f, 200.0f, 80.0f}, 0.0f, -1.0f, {0.0f, 150.0f, 200.0f, 400.0f, 600.0f}, 680.0f},
    {{{S::autoSize, 0.0f, S::fr, 0.5f}, {S::autoSize, 0.0f, S::percent, 10.0f}}, {100.0f, 200.0f, 80.0f, 200.0f}, 10.0f, -1.0f, {0.0f, 110.0f, 320.0f, 410.0f}, 610.0f},
    {{}, {100.0f, 50.0f}, 10.0f, 300.0f, {0.0f, 110.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::percent, 10.0f, S::percent, 10.0f}, {S::autoSize, 0.0f, S::percent, 25.0f}}, {20.0f, 20.0f, 80.0f, 20.0f, 200.0f}, 0.0f, -1.0f, {0.0f, 20.0f, 54.0f, 134.0f, 154.0f}, 340.0f},
    {{{S::points, 0.0f, S::points, 0.0f}, {S::percent, 25.0f, S::fr, 0.5f}}, {100.0f, 50.0f}, 10.0f, -1.0f, {0.0f, 10.0f}, 60.0f},
    {{{S::points, 80.0f, S::percent, 50.0f}, {S::points, 80.0f, S::points, 80.0f}, {S::autoSize, 0.0f, S::points, 120.0f}}, {50.0f, 20.0f, 20.0f}, 10.0f, -1.0f, {0.0f, 90.0f, 180.0f}, 200.0f},
    {{{S::points, 0.0f, S::autoSize, 0.0f}, {S::points, 0.0f, S::percent, 25.0f}, {S::percent, 25.0f, S::percent, 50.0f}}, {50.0f, 200.0f, 50.0f}, 10.0f, 400.0f, {0.0f, 60.0f, 170.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::percent, 10.0f}, {S::percent, 25.0f, S::autoSize, 0.0f}}, {80.0f, 150.0f, 20.0f, 200.0f}, 10.0f, -1.0f, {0.0f, 90.0f, 250.0f, 280.0f}, 480.0f},
    {{{S::autoSize, 0.0f, S::fr, 2.0f}, {S::autoSize, 0.0f, S::percent, 25.0f}, {S::autoSize, 0.0f, S::autoSize, 0.0f}}, {100.0f, 80.0f, 50.0f}, 10.0f, 300.0f, {0.0f, 160.0f, 250.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::fr, 0.5f}, {S::points, 40.0f, S::points, 40.0f}}, {150.0f, 100.0f, 20.0f, 20.0f, 100.0f}, 10.0f, 300.0f, {0.0f, 160.0f, 210.0f, 240.0f, 270.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::fr, 2.0f}, {S::points, 80.0f, S::percent, 50.0f}}, {100.0f, 80.0f, 50.0f, 80.0f}, 10.0f, 300.0f, {0.0f, 110.0f, 200.0f, 260.0f}, -1.0f},
    {{{S::points, 80.0f, S::fr, 1.0f}, {S::points, 80.0f, S::fr, 2.0f}, {S::points, 40.0f, S::points, 40.0f}}, {100.0f, 150.0f, 20.0f, 80.0f, 50.0f}, 10.0f, -1.0f, {0.0f, 110.0f, 320.0f, 370.0f, 460.0f}, 510.0f},
    {{{S::autoSize, 0.0f, S::points, 0.0f}}, {200.0f, 200.0f, 80.0f, 100.0f, 20.0f, 150.0f}, 0.0f, -1.0f, {0.0f, 200.0f, 400.0f, 480.0f, 580.0f, 600.0f}, 750.0f},
    {{{S::percent, 50.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::fr, 1.0f}}, {20.0f, 100.0f, 50.0f}, 10.0f, 400.0f, {0.0f, 210.0f, 350.0f}, -1.0f},
    {{{S::percent, 25.0f, S::percent, 25.0f}, {S::percent, 50.0f, S::points, 80.0f}, {S::autoSize, 0.0f, S::autoSize, 0.0f}}, {150.0f, 150.0f}, 10.0f, 300.0f, {0.0f, 85.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::percent, 10.0f}}, {200.0f, 80.0f}, 0.0f, -1.0f, {0.0f, 200.0f}, 280.0f},
    {{{S::autoSize, 0.0f, S::fr, 2.0f}}, {100.0f, 200.0f, 100.0f}, 0.0f, 400.0f, {0.0f, 100.0f, 300.0f}, -1.0f},
    {{{S::points, 40.0f, S::points, 40.0f}}, {20.0f, 200.0f}, 10.0f, 400.0f, {0.0f, 50.0f}, -1.0f},
    {{{S::points, 80.0f, S::fr, 0.5f}, {S::points, 40.0f, S::points, 0.0f}, {S::points, 120.0f, S::percent, 50.0f}}, {150.0f, 100.0f, 50.0f}, 10.0f, -1.0f, {0.0f, 90.0f, 140.0f}, 260.0f},
    {{{S::autoSize, 0.0f, S::fr, 0.5f}, {S::percent, 10.0f, S::points, 120.0f}}, {150.0f}, 10.0f, 400.0f, {0.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}}, {50.0f, 80.0f}, 0.0f, 300.0f, {0.0f, 50.0f}, -1.0f},
    {{}, {20.0f, 80.0f}, 10.0f, -1.0f, {0.0f, 30.0f}, 110.0f},
    {{{S::percent, 25.0f, S::percent, 50.0f}}, {50.0f, 80.0f, 100.0f, 80.0f, 20.0f, 20.0f}, 10.0f, 300.0f, {0.0f, 85.0f, 175.0f, 285.0f, 375.0f, 405.0f}, -1.0f},
    {{{S::percent, 10.0f, S::autoSize, 0.0f}}, {100.0f, 100.0f, 80.0f, 150.0f, 20.0f, 50.0f}, 0.0f, 400.0f, {0.0f, 40.0f, 140.0f, 220.0f, 370.0f, 390.0f}, -1.0f},
    {{{S::points, 120.0f, S::points, 120.0f}}, {50.0f, 100.0f, 20.0f, 150.0f, 200.0f}, 10.0f, 400.0f, {0.0f, 130.0f, 240.0f, 270.0f, 430.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::points, 120.0f}, {S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::fr, 1.0f}}, {200.0f}, 0.0f, 400.0f, {0.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::points, 120.0f}, {S::points, 120.0f, S::points, 120.0f}, {S::percent, 25.0f, S::percent, 25.0f}}, {150.0f, 150.0f, 200.0f}, 10.0f, 400.0f, {0.0f, 160.0f, 290.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::percent, 25.0f, S::percent, 10.0f}, {S::autoSize, 0.0f, S::autoSize, 0.0f}}, {80.0f, 100.0f}, 10.0f, 300.0f, {0.0f, 90.0f}, -1.0f},
    {{}, {50.0f, 80.0f, 50.0f, 200.0f, 150.0f}, 0.0f, 400.0f, {0.0f, 50.0f, 130.0f, 180.0f, 380.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::points, 120.0f}}, {150.0f, 20.0f, 20.0f, 50.0f}, 10.0f, 400.0f, {0.0f, 160.0f, 290.0f, 320.0f}, -1.0f},
    {{}, {200.0f, 50.0f, 50.0f}, 10.0f, 300.0f, {0.0f, 210.0f, 270.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::fr, 2.0f}, {S::points, 120.0f, S::points, 120.0f}}, {100.0f, 80.0f, 50.0f, 20.0f, 50.0f}, 10.0f, 300.0f, {0.0f, 110.0f, 200.0f, 330.0f, 360.0f}, -1.0f},
    {{{S::points, 80.0f, S::percent, 10.0f}, {S::autoSize, 0.0f, S::fr, 0.5f}, {S::autoSize, 0.0f, S::fr, 0.5f}}, {80.0f, 80.0f, 20.0f, 80.0f, 100.0f, 50.0f}, 0.0f, 400.0f, {0.0f, 80.0f, 160.0f, 180.0f, 260.0f, 360.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::percent, 10.0f}}, {200.0f, 100.0f}, 0.0f, 400.0f, {0.0f, 200.0f}, -1.0f},
    {{}, {80.0f, 100.0f, 20.0f}, 10.0f, -1.0f, {0.0f, 90.0f, 200.0f}, 220.0f},
    {{}, {200.0f}, 0.0f, 400.0f, {0.0f}, -1.0f},
    {{{S::percent, 50.0f, S::percent, 50.0f}, {S::autoSize, 0.0f, S::fr, 2.0f}}, {150.0f, 50.0f, 20.0f, 150.0f, 20.0f, 150.0f}, 0.0f, 400.0f, {0.0f, 200.0f, 250.0f, 270.0f, 420.0f, 440.0f}, -1.0f},
    {{{S::percent, 10.0f, S::percent, 10.0f}, {S::percent, 50.0f, S::fr, 1.0f}, {S::points, 0.0f, S::points, 0.0f}}, {100.0f, 200.0f, 200.0f, 150.0f}, 10.0f, 300.0f, {0.0f, 40.0f, 200.0f, 210.0f}, -1.0f},
    {{{S::points, 40.0f, S::points, 40.0f}, {S::percent, 25.0f, S::percent, 10.0f}, {S::autoSize, 0.0f, S::fr, 2.0f}}, {50.0f, 80.0f, 80.0f, 20.0f}, 0.0f, 400.0f, {0.0f, 40.0f, 140.0f, 380.0f}, -1.0f},
    {{{S::percent, 10.0f, S::autoSize, 0.0f}}, {50.0f, 20.0f, 100.0f, 50.0f, 100.0f, 150.0f}, 10.0f, 400.0f, {0.0f, 50.0f, 80.0f, 190.0f, 250.0f, 360.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::fr, 2.0f}, {S::percent, 10.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::percent, 10.0f}}, {200.0f, 80.0f}, 10.0f, -1.0f, {0.0f, 210.0f}, 300.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::fr, 1.0f}, {S::percent, 50.0f, S::percent, 50.0f}}, {200.0f, 20.0f, 50.0f, 200.0f, 150.0f}, 0.0f, -1.0f, {0.0f, 200.0f, 220.0f, 530.0f, 730.0f}, 620.0f},
    {{{S::autoSize, 0.0f, S::points, 80.0f}, {S::autoSize, 0.0f, S::autoSize, 0.0f}}, {20.0f, 200.0f, 200.0f}, 0.0f, 300.0f, {0.0f, 20.0f, 220.0f}, -1.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::fr, 0.5f}, {S::autoSize, 0.0f, S::percent, 50.0f}}, {80.0f, 150.0f, 20.0f}, 10.0f, 300.0f, {0.0f, 90.0f, 250.0f}, -1.0f},
    {{{S::points, 40.0f, S::points, 40.0f}}, {200.0f}, 10.0f, 400.0f, {0.0f}, -1.0f},
    {{}, {200.0f, 80.0f}, 0.0f, 300.0f, {0.0f, 200.0f}, -1.0f},
    {{{S::percent, 50.0f, S::points, 120.0f}, {S::autoSize, 0.0f, S::fr, 2.0f}}, {50.0f, 50.0f, 150.0f, 20.0f, 20.0f, 150.0f}, 10.0f, -1.0f, {0.0f, 255.0f, 315.0f, 475.0f, 505.0f, 535.0f}, 490.0f},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::fr, 1.0f}}, {50.0f, 150.0f, 50.0f}, 10.0f, 300.0f, {0.0f, 60.0f, 250.0f}, -1.0f},
    // The cases each rule was found from: fr rows under 1, an fr row whose
    // items would leave it under its minimum, and percents sized again in a
    // hugging grid's height.
    {{{S::points, 0.0f, S::fr, 0.5f}}, {80.0f, 100.0f, 200.0f, 200.0f}, 0.0f,
     -1.0f, {0.0f, 40.0f, 140.0f, 340.0f}, 540.0f},
    {{{S::autoSize, 0.0f, S::fr, 0.5f}, {S::autoSize, 0.0f, S::fr, 0.5f}},
     {200.0f, 50.0f, 50.0f, 80.0f, 80.0f, 20.0f}, 10.0f, -1.0f,
     {0.0f, 210.0f, 320.0f, 380.0f, 470.0f, 560.0f}, 580.0f},
    {{{S::points, 120.0f, S::fr, 2.0f}, {S::autoSize, 0.0f, S::points, 120.0f}},
     {200.0f, 50.0f}, 0.0f, -1.0f, {0.0f, 200.0f}, 250.0f},
    {{{S::autoSize, 0.0f, S::fr, 2.0f}, {S::points, 120.0f, S::fr, 0.5f},
      {S::autoSize, 0.0f, S::points, 80.0f}},
     {50.0f, 200.0f, 100.0f}, 10.0f, -1.0f, {0.0f, 250.0f, 380.0f}, 480.0f},
    {{{S::percent, 10.0f, S::autoSize, 0.0f}}, {150.0f, 20.0f}, 0.0f, -1.0f,
     {0.0f, 150.0f}, 170.0f},
    {{{S::percent, 10.0f, S::percent, 10.0f}, {S::autoSize, 0.0f, S::fr, 2.0f},
      {S::percent, 50.0f, S::fr, 0.5f}},
     {200.0f, 80.0f, 80.0f, 20.0f, 100.0f}, 10.0f, -1.0f,
     {0.0f, 70.0f, 160.0f, 470.0f, 500.0f}, 600.0f},
};
// clang-format on
} // namespace

TEST_CASE("VirtualLayout grid rows land where layout puts them",
          "[virtual_layout]")
{
    int index = 0;
    for (auto& rowCase : rowCases)
    {
        VirtualLayout layout;
        layout.beginGrid(rowCase.gap, 1, LayoutCrossAlign::start);
        layout.gridColumnStarts() = {0, 100};
        layout.gridRows().templates = rowCase.rows;
        layout.gridRows().space = rowCase.rowSpace;
        layout.beginSegment();
        for (float height : rowCase.heights)
        {
            layout.addItem(height, 100);
        }
        layout.end();
        INFO("case " << index++);
        for (int item = 0; item < (int)rowCase.tops.size(); item++)
        {
            float top = layout.lineStart(layout.lineOfItem(item)) +
                        layout.itemLineOffset(item);
            CHECK(top == Approx(rowCase.tops[item]).margin(0.01f));
        }
        if (rowCase.height >= 0.0f)
        {
            CHECK(layout.extent() == Approx(rowCase.height).margin(0.01f));
        }
    }
}

namespace
{
// Column tracks as layout sizes them for two rows of items, recorded from the
// layout engine by virtual_layout_grid_rows_test.dart: column tracks, item
// widths, column gap, the grid's inner width (-1 when it hugs its columns),
// whether its width is a flex parent's main axis and where layout put each
// column.
struct ColumnCase
{
    std::vector<VirtualGridTrack> columns;
    std::vector<float> widths;
    float gap;
    float columnSpace;
    bool resize;
    std::vector<float> starts;
};

// clang-format off
const ColumnCase columnCases[] = {
    {{{S::autoSize, 0.0f, S::points, 0.0f}, {S::percent, 10.0f, S::fr, 0.5f}}, {20.0f, 100.0f, 150.0f, 50.0f}, 10.0f, 300.0f, true, {0.0f, 160.0f}},
    {{{S::points, 80.0f, S::fr, 1.0f}, {S::autoSize, 0.0f, S::fr, 1.0f}}, {100.0f, 50.0f, 100.0f, 50.0f}, 10.0f, 400.0f, true, {0.0f, 205.0f}},
    {{{S::points, 40.0f, S::points, 40.0f}, {S::autoSize, 0.0f, S::fr, 0.5f}}, {20.0f, 50.0f, 20.0f, 50.0f}, 10.0f, 400.0f, false, {0.0f, 50.0f}},
    {{{S::autoSize, 0.0f, S::fr, 1.0f}, {S::autoSize, 0.0f, S::fr, 1.0f}, {S::points, 40.0f, S::fr, 0.5f}}, {20.0f, 80.0f, 100.0f, 100.0f, 100.0f, 100.0f}, 10.0f, 400.0f, true, {0.0f, 162.0f, 324.0f}},
    {{{S::autoSize, 0.0f, S::points, 80.0f}, {S::autoSize, 0.0f, S::points, 80.0f}}, {50.0f, 100.0f, 50.0f, 100.0f}, 0.0f, 300.0f, false, {0.0f, 80.0f}},
    {{{S::autoSize, 0.0f, S::points, 120.0f}, {S::autoSize, 0.0f, S::fr, 2.0f}, {S::points, 120.0f, S::autoSize, 0.0f}}, {80.0f, 80.0f, 20.0f, 100.0f, 150.0f, 20.0f}, 0.0f, 400.0f, false, {0.0f, 120.0f, 280.0f}},
    {{{S::autoSize, 0.0f, S::points, 80.0f}, {S::points, 40.0f, S::percent, 50.0f}, {S::percent, 25.0f, S::percent, 25.0f}}, {20.0f, 150.0f, 100.0f, 80.0f, 100.0f, 150.0f}, 0.0f, 300.0f, true, {0.0f, 80.0f, 225.0f}},
    {{{S::autoSize, 0.0f, S::fr, 1.0f}, {S::points, 0.0f, S::points, 0.0f}, {S::autoSize, 0.0f, S::autoSize, 0.0f}}, {50.0f, 20.0f, 50.0f, 80.0f, 50.0f, 150.0f}, 0.0f, 300.0f, false, {0.0f, 150.0f, 150.0f}},
    {{{S::points, 40.0f, S::points, 80.0f}, {S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::points, 40.0f}}, {50.0f, 150.0f, 80.0f, 100.0f, 80.0f, 20.0f}, 10.0f, -1.0f, true, {0.0f, 50.0f, 210.0f}},
    {{{S::autoSize, 0.0f, S::points, 0.0f}, {S::autoSize, 0.0f, S::fr, 0.5f}}, {50.0f, 20.0f, 80.0f, 50.0f}, 0.0f, -1.0f, true, {0.0f, 80.0f}},
    {{{S::points, 0.0f, S::fr, 0.5f}, {S::percent, 25.0f, S::fr, 1.0f}}, {50.0f, 20.0f, 150.0f, 80.0f}, 0.0f, 300.0f, true, {0.0f, 100.0f}},
    {{{S::percent, 50.0f, S::points, 120.0f}, {S::autoSize, 0.0f, S::autoSize, 0.0f}}, {50.0f, 150.0f, 50.0f, 150.0f}, 10.0f, -1.0f, false, {0.0f, 115.0f}},
    {{{S::autoSize, 0.0f, S::autoSize, 0.0f}, {S::autoSize, 0.0f, S::autoSize, 0.0f}}, {80.0f, 100.0f, 100.0f, 50.0f}, 0.0f, 400.0f, true, {0.0f, 100.0f}},
    {{{S::autoSize, 0.0f, S::fr, 2.0f}, {S::percent, 10.0f, S::percent, 10.0f}}, {50.0f, 150.0f, 20.0f, 150.0f}, 10.0f, 400.0f, true, {0.0f, 360.0f}},
    {{{S::autoSize, 0.0f, S::points, 120.0f}, {S::points, 120.0f, S::points, 120.0f}, {S::autoSize, 0.0f, S::fr, 2.0f}}, {20.0f, 80.0f, 150.0f, 80.0f, 150.0f, 80.0f}, 0.0f, 400.0f, true, {0.0f, 120.0f, 240.0f}},
    {{{S::percent, 50.0f, S::percent, 50.0f}, {S::autoSize, 0.0f, S::fr, 1.0f}}, {150.0f, 100.0f, 50.0f, 20.0f}, 0.0f, -1.0f, false, {0.0f, 125.0f}},
    {{{S::points, 0.0f, S::points, 120.0f}, {S::autoSize, 0.0f, S::fr, 2.0f}, {S::autoSize, 0.0f, S::fr, 1.0f}}, {20.0f, 80.0f, 20.0f, 150.0f, 50.0f, 20.0f}, 0.0f, -1.0f, true, {0.0f, 20.0f, 100.0f}},
    {{{S::autoSize, 0.0f, S::points, 80.0f}, {S::points, 0.0f, S::fr, 0.5f}, {S::points, 120.0f, S::points, 120.0f}}, {150.0f, 50.0f, 20.0f, 20.0f, 100.0f, 150.0f}, 10.0f, -1.0f, true, {0.0f, 160.0f, 195.0f}},
    {{{S::points, 80.0f, S::autoSize, 0.0f}, {S::points, 0.0f, S::fr, 0.5f}}, {100.0f, 150.0f, 150.0f, 50.0f}, 0.0f, -1.0f, true, {0.0f, 150.0f}},
};
// clang-format on
} // namespace

TEST_CASE("VirtualLayout grid columns land where layout puts them",
          "[virtual_layout]")
{
    int index = 0;
    for (auto& columnCase : columnCases)
    {
        int count = (int)columnCase.columns.size();
        VirtualLayout layout;
        layout.beginGrid(10, count, LayoutCrossAlign::start, columnCase.gap);
        layout.gridColumns().templates = columnCase.columns;
        layout.gridColumns().space = columnCase.columnSpace;
        layout.gridColumns().resize = columnCase.resize;
        layout.beginSegment();
        for (float width : columnCase.widths)
        {
            layout.addItem(50, width);
        }
        layout.end();
        INFO("case " << index++);
        float start = 0.0f;
        for (int column = 0; column < count; column++)
        {
            CHECK(start == Approx(columnCase.starts[column]).margin(0.01f));
            start += layout.gridColumnSize(column) + columnCase.gap;
        }
    }
}
