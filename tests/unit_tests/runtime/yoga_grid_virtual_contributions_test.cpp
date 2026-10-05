#include "yoga/YGNode.h"
#include "yoga/Yoga.h"
#include <catch.hpp>
#include <cmath>
#include <vector>

// A virtualized grid gives layout its unrealized items as per row and column
// contributions (YGNodeSetGridVirtualContributions); tracks must size exactly
// as the items themselves would. The track lists are the ones
// virtual_layout_grid_rows_test.dart recorded from the layout engine.

namespace
{
struct Track
{
    YGGridTrackType minType;
    float minValue;
    YGGridTrackType maxType;
    float maxValue;
};
using Tracks = std::vector<Track>;
constexpr auto A = YGGridTrackTypeAuto;
constexpr auto P = YGGridTrackTypePoints;
constexpr auto PC = YGGridTrackTypePercent;
constexpr auto F = YGGridTrackTypeFr;

// clang-format off
const Tracks rowTracks[] = {
    {{A, 0, A, 0}, {A, 0, P, 40}, {P, 80, P, 120}},
    {},
    {{A, 0, F, 2}, {PC, 10, PC, 10}, {P, 80, P, 80}},
    {{A, 0, A, 0}},
    {{A, 0, PC, 10}, {A, 0, A, 0}},
    {{P, 0, P, 0}},
    {{P, 40, P, 40}},
    {{P, 0, PC, 10}},
    {{A, 0, A, 0}, {A, 0, F, 0.5f}},
    {{A, 0, A, 0}, {A, 0, A, 0}, {A, 0, F, 1}},
    {{A, 0, F, 1}, {P, 120, P, 80}, {A, 0, P, 40}},
    {{PC, 25, PC, 25}},
    {{A, 0, P, 80}},
    {{A, 0, F, 1}, {A, 0, P, 80}},
    {{P, 80, F, 2}},
    {{P, 0, P, 0}, {A, 0, A, 0}, {PC, 50, PC, 50}},
    {{A, 0, F, 1}, {A, 0, F, 0.5f}, {P, 80, P, 0}},
    {{PC, 10, PC, 10}},
    {{PC, 25, PC, 50}, {P, 0, P, 120}, {A, 0, F, 0.5f}},
    {{P, 120, P, 120}},
    {{A, 0, F, 1}, {PC, 25, P, 80}},
    {{A, 0, F, 0.5f}, {A, 0, PC, 10}},
    {{A, 0, A, 0}, {PC, 10, PC, 10}, {A, 0, PC, 25}},
    {{P, 0, P, 0}, {PC, 25, F, 0.5f}},
    {{P, 80, PC, 50}, {P, 80, P, 80}, {A, 0, P, 120}},
    {{P, 0, A, 0}, {P, 0, PC, 25}, {PC, 25, PC, 50}},
    {{A, 0, PC, 10}, {PC, 25, A, 0}},
    {{A, 0, F, 2}, {A, 0, PC, 25}, {A, 0, A, 0}},
    {{A, 0, F, 0.5f}, {P, 40, P, 40}},
    {{A, 0, F, 2}, {P, 80, PC, 50}},
    {{P, 80, F, 1}, {P, 80, F, 2}, {P, 40, P, 40}},
    {{A, 0, P, 0}},
    {{PC, 50, A, 0}, {A, 0, F, 1}},
    {{PC, 25, PC, 25}, {PC, 50, P, 80}, {A, 0, A, 0}},
    {{A, 0, A, 0}, {A, 0, A, 0}, {A, 0, PC, 10}},
    {{A, 0, F, 2}},
    {{P, 80, F, 0.5f}, {P, 40, P, 0}, {P, 120, PC, 50}},
    {{A, 0, F, 0.5f}, {PC, 10, P, 120}},
    {{PC, 25, PC, 50}},
    {{PC, 10, A, 0}},
    {{A, 0, P, 120}, {A, 0, A, 0}, {A, 0, F, 1}},
    {{A, 0, P, 120}, {P, 120, P, 120}, {PC, 25, PC, 25}},
    {{A, 0, A, 0}, {PC, 25, PC, 10}, {A, 0, A, 0}},
    {{A, 0, A, 0}, {A, 0, P, 120}},
    {{A, 0, A, 0}, {A, 0, F, 2}, {P, 120, P, 120}},
    {{P, 80, PC, 10}, {A, 0, F, 0.5f}, {A, 0, F, 0.5f}},
    {{A, 0, A, 0}, {A, 0, PC, 10}},
    {{PC, 50, PC, 50}, {A, 0, F, 2}},
    {{PC, 10, PC, 10}, {PC, 50, F, 1}, {P, 0, P, 0}},
    {{P, 40, P, 40}, {PC, 25, PC, 10}, {A, 0, F, 2}},
    {{A, 0, F, 2}, {PC, 10, A, 0}, {A, 0, PC, 10}},
    {{A, 0, A, 0}, {A, 0, F, 1}, {PC, 50, PC, 50}},
    {{A, 0, P, 80}, {A, 0, A, 0}},
    {{A, 0, A, 0}, {A, 0, F, 0.5f}, {A, 0, PC, 50}},
    {{PC, 50, P, 120}, {A, 0, F, 2}},
    {{A, 0, A, 0}, {A, 0, F, 1}},
    {{P, 0, F, 0.5f}},
    {{A, 0, F, 0.5f}, {A, 0, F, 0.5f}},
    {{P, 120, F, 2}, {A, 0, P, 120}},
    {{A, 0, F, 2}, {P, 120, F, 0.5f}, {A, 0, P, 80}},
    {{PC, 10, PC, 10}, {A, 0, F, 2}, {PC, 50, F, 0.5f}},
};

const Tracks columnTracks[] = {
    {{A, 0, P, 0}, {PC, 10, F, 0.5f}},
    {{P, 80, F, 1}, {A, 0, F, 1}},
    {{P, 40, P, 40}, {A, 0, F, 0.5f}},
    {{A, 0, F, 1}, {A, 0, F, 1}, {P, 40, F, 0.5f}},
    {{A, 0, P, 80}, {A, 0, P, 80}},
    {{A, 0, P, 120}, {A, 0, F, 2}, {P, 120, A, 0}},
    {{A, 0, P, 80}, {P, 40, PC, 50}, {PC, 25, PC, 25}},
    {{A, 0, F, 1}, {P, 0, P, 0}, {A, 0, A, 0}},
    {{P, 40, P, 80}, {A, 0, A, 0}, {A, 0, P, 40}},
    {{A, 0, P, 0}, {A, 0, F, 0.5f}},
    {{P, 0, F, 0.5f}, {PC, 25, F, 1}},
    {{PC, 50, P, 120}, {A, 0, A, 0}},
    {{A, 0, A, 0}, {A, 0, A, 0}},
    {{A, 0, F, 2}, {PC, 10, PC, 10}},
    {{A, 0, P, 120}, {P, 120, P, 120}, {A, 0, F, 2}},
    {{PC, 50, PC, 50}, {A, 0, F, 1}},
    {{P, 0, P, 120}, {A, 0, F, 2}, {A, 0, F, 1}},
    {{A, 0, P, 80}, {P, 0, F, 0.5f}, {P, 120, P, 120}},
    {{P, 80, A, 0}, {P, 0, F, 0.5f}},
};
// clang-format on

enum class Sizing
{
    fixed,
    hug,
    // Hugs inside a flex parent along the scroll axis, which lays it out again
    // in the size it reports.
    flexParent,
};

void setTracks(YGNodeRef node, const Tracks& tracks, bool rows)
{
    rows ? YGNodeStyleSetGridTemplateRowsCount(node, tracks.size())
         : YGNodeStyleSetGridTemplateColumnsCount(node, tracks.size());
    for (size_t i = 0; i < tracks.size(); i++)
    {
        const Track& t = tracks[i];
        if (t.minType == t.maxType && t.minValue == t.maxValue)
        {
            rows ? YGNodeStyleSetGridTemplateRow(node, i, t.minType, t.minValue)
                 : YGNodeStyleSetGridTemplateColumn(node,
                                                    i,
                                                    t.minType,
                                                    t.minValue);
        }
        else
        {
            rows ? YGNodeStyleSetGridTemplateRowMinMax(node,
                                                       i,
                                                       t.minType,
                                                       t.minValue,
                                                       t.maxType,
                                                       t.maxValue)
                 : YGNodeStyleSetGridTemplateColumnMinMax(node,
                                                          i,
                                                          t.minType,
                                                          t.minValue,
                                                          t.maxType,
                                                          t.maxValue);
        }
    }
}

struct Grid
{
    const Tracks* rows = nullptr;
    const Tracks* columns = nullptr;
    int columnCount = 1;
    std::vector<float> widths;
    std::vector<float> heights;
    float gap = 0.0f;
    Sizing sizing = Sizing::fixed;
    // Along the scroll axis: rows for a row case, columns for a column case.
    bool vertical = true;
};

// Lays the grid out with items as nodes for the cells in realized, the rest as
// contributions, and returns its row and column lines then its size.
std::vector<float> solve(const Grid& grid, const std::vector<bool>& realized)
{
    YGNodeRef node = YGNodeNew();
    YGNodeStyleSetDisplay(node, YGDisplayGrid);
    YGNodeStyleSetGap(node, YGGutterAll, grid.gap);
    if (grid.rows != nullptr)
    {
        setTracks(node, *grid.rows, true);
    }
    if (grid.columns != nullptr)
    {
        setTracks(node, *grid.columns, false);
    }
    float space = grid.vertical ? 400.0f : 300.0f;
    float across = grid.vertical ? 300.0f : 400.0f;
    grid.vertical ? YGNodeStyleSetWidth(node, across)
                  : YGNodeStyleSetHeight(node, across);
    if (grid.sizing == Sizing::fixed)
    {
        grid.vertical ? YGNodeStyleSetHeight(node, space)
                      : YGNodeStyleSetWidth(node, space);
    }
    int count = (int)grid.widths.size();
    int rowCount = (count + grid.columnCount - 1) / grid.columnCount;
    std::vector<float> rowSizes(rowCount, 0.0f);
    std::vector<float> columnSizes(grid.columnCount, 0.0f);
    uint32_t childIndex = 0;
    for (int i = 0; i < count; i++)
    {
        int row = i / grid.columnCount;
        int column = i % grid.columnCount;
        rowSizes[row] = std::max(rowSizes[row], grid.heights[i]);
        columnSizes[column] = std::max(columnSizes[column], grid.widths[i]);
        if (!realized[i])
        {
            continue;
        }
        YGNodeRef item = YGNodeNew();
        YGNodeStyleSetWidth(item, grid.widths[i]);
        YGNodeStyleSetHeight(item, grid.heights[i]);
        // Realized items take their own cell, as the virtualizer pins them.
        YGNodeStyleSetGridRowStart(item, row + 1);
        YGNodeStyleSetGridColumnStart(item, column + 1);
        YGNodeInsertChild(node, item, childIndex++);
    }
    bool anyVirtual = false;
    for (bool isRealized : realized)
    {
        anyVirtual = anyVirtual || !isRealized;
    }
    if (anyVirtual)
    {
        YGNodeSetGridVirtualContributions(node,
                                          rowSizes.data(),
                                          rowSizes.size(),
                                          columnSizes.data(),
                                          columnSizes.size());
    }
    YGNodeRef root = node;
    if (grid.sizing == Sizing::flexParent)
    {
        root = YGNodeNew();
        YGNodeStyleSetFlexDirection(root,
                                    grid.vertical ? YGFlexDirectionColumn
                                                  : YGFlexDirectionRow);
        YGNodeInsertChild(root, node, 0);
    }
    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
    std::vector<float> out;
    for (uint32_t i = 0; i < YGNodeLayoutGetGridRowLineCount(node); i++)
    {
        out.push_back(YGNodeLayoutGetGridRowLineOffset(node, i));
    }
    out.push_back(-1.0f);
    for (uint32_t i = 0; i < YGNodeLayoutGetGridColumnLineCount(node); i++)
    {
        out.push_back(YGNodeLayoutGetGridColumnLineOffset(node, i));
    }
    out.push_back(YGNodeLayoutGetWidth(node));
    out.push_back(YGNodeLayoutGetHeight(node));
    YGNodeFreeRecursive(root);
    return out;
}

void checkSame(const std::vector<float>& expected,
               const std::vector<float>& actual)
{
    REQUIRE(actual.size() == expected.size());
    for (size_t i = 0; i < expected.size(); i++)
    {
        INFO("value " << i);
        CHECK(actual[i] == Approx(expected[i]).margin(0.01f));
    }
}

const float itemSizes[] = {150, 50, 80, 200, 20, 100, 80, 150};

// Every grid both ways: all items as nodes, then a few realized and the rest
// as contributions, then none realized.
void checkGrid(const Grid& grid)
{
    int count = (int)grid.widths.size();
    auto expected = solve(grid, std::vector<bool>(count, true));
    std::vector<bool> some(count, false);
    for (int i = count / 3; i < count / 3 + grid.columnCount && i < count; i++)
    {
        some[i] = true;
    }
    {
        INFO("some realized");
        checkSame(expected, solve(grid, some));
    }
    {
        INFO("none realized");
        checkSame(expected, solve(grid, std::vector<bool>(count, false)));
    }
}
} // namespace

TEST_CASE("Grid virtual contributions size rows as their items would",
          "[layout][grid]")
{
    int index = 0;
    for (auto& rows : rowTracks)
    {
        for (auto sizing : {Sizing::fixed, Sizing::hug, Sizing::flexParent})
        {
            Grid grid;
            grid.rows = &rows;
            grid.gap = (index % 2) * 10.0f;
            grid.sizing = sizing;
            int count = (int)rows.size() + 3;
            for (int i = 0; i < count; i++)
            {
                grid.widths.push_back(100.0f);
                grid.heights.push_back(itemSizes[(index + i) % 8]);
            }
            INFO("row case " << index << " sizing " << (int)sizing);
            checkGrid(grid);
        }
        index++;
    }
}

TEST_CASE("Grid virtual contributions size columns as their items would",
          "[layout][grid]")
{
    int index = 0;
    for (auto& columns : columnTracks)
    {
        for (auto sizing : {Sizing::fixed, Sizing::hug, Sizing::flexParent})
        {
            Grid grid;
            grid.columns = &columns;
            grid.columnCount = (int)columns.size();
            grid.gap = (index % 2) * 10.0f;
            grid.sizing = sizing;
            grid.vertical = false;
            for (int i = 0; i < grid.columnCount * 3; i++)
            {
                grid.widths.push_back(itemSizes[(index + i) % 8]);
                grid.heights.push_back(50.0f);
            }
            INFO("column case " << index << " sizing " << (int)sizing);
            checkGrid(grid);
        }
        index++;
    }
}

TEST_CASE("Grid virtual contributions extend the implicit grid",
          "[layout][grid]")
{
    YGNodeRef node = YGNodeNew();
    YGNodeStyleSetDisplay(node, YGDisplayGrid);
    YGNodeStyleSetWidth(node, 200);
    float rows[] = {10, 20, 30};
    float columns[] = {50, 60};
    REQUIRE(YGNodeSetGridVirtualContributions(node, rows, 3, columns, 2));
    // The same contributions again change nothing.
    CHECK_FALSE(YGNodeSetGridVirtualContributions(node, rows, 3, columns, 2));
    YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
    CHECK(YGNodeLayoutGetGridRowLineCount(node) == 4);
    CHECK(YGNodeLayoutGetGridColumnLineCount(node) == 3);
    CHECK(YGNodeLayoutGetHeight(node) == Approx(60.0f));
    // Clearing them leaves an empty container.
    REQUIRE(YGNodeSetGridVirtualContributions(node, nullptr, 0, nullptr, 0));
    YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
    CHECK(YGNodeLayoutGetHeight(node) == Approx(0.0f));
    YGNodeFree(node);
}

namespace
{
std::vector<float> rowLines(YGNodeRef node)
{
    std::vector<float> lines;
    for (uint32_t i = 0; i < YGNodeLayoutGetGridRowLineCount(node); i++)
    {
        lines.push_back(YGNodeLayoutGetGridRowLineOffset(node, i));
    }
    return lines;
}

std::vector<float> columnLines(YGNodeRef node)
{
    std::vector<float> lines;
    for (uint32_t i = 0; i < YGNodeLayoutGetGridColumnLineCount(node); i++)
    {
        lines.push_back(YGNodeLayoutGetGridColumnLineOffset(node, i));
    }
    return lines;
}
} // namespace

TEST_CASE("Grid virtual sizes are reused only while their inputs hold",
          "[layout][grid]")
{
    YGNodeRef node = YGNodeNew();
    YGNodeStyleSetDisplay(node, YGDisplayGrid);
    YGNodeStyleSetWidth(node, 300);
    YGNodeStyleSetGridTemplateColumnsCount(node, 2);
    YGNodeStyleSetGridTemplateColumn(node, 0, YGGridTrackTypeFr, 1);
    YGNodeStyleSetGridTemplateColumn(node, 1, YGGridTrackTypeAuto, 0);
    std::vector<float> rows = {40, 50, 60, 70, 80};
    std::vector<float> columns = {100, 90};
    YGNodeSetGridVirtualContributions(node,
                                      rows.data(),
                                      rows.size(),
                                      columns.data(),
                                      columns.size());
    YGNodeRef item = YGNodeNew();
    YGNodeStyleSetHeight(item, 40);
    YGNodeStyleSetGridRowStart(item, 1);
    YGNodeStyleSetGridColumnStart(item, 1);
    YGNodeInsertChild(node, item, 0);
    YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
    auto rowsBefore = rowLines(node);
    auto columnsBefore = columnLines(node);
    std::vector<float> expectedRows = {0, 40, 90, 150, 220, 300};
    CHECK(rowsBefore == expectedRows);

    SECTION("realized items moving, as they do while scrolling")
    {
        YGNodeStyleSetGridRowStart(item, 4);
        YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
        CHECK(rowLines(node) == rowsBefore);
        CHECK(columnLines(node) == columnsBefore);
        // The item lays out in its new cell.
        CHECK(YGNodeLayoutGetTop(item) == Approx(150.0f));
    }
    SECTION("contributions changing")
    {
        rows[1] = 100;
        YGNodeSetGridVirtualContributions(node,
                                          rows.data(),
                                          rows.size(),
                                          columns.data(),
                                          columns.size());
        YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
        std::vector<float> expected = {0, 40, 140, 200, 270, 350};
        CHECK(rowLines(node) == expected);
    }
    SECTION("style changing")
    {
        YGNodeStyleSetGap(node, YGGutterRow, 10);
        YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
        std::vector<float> expected = {0, 50, 110, 180, 260, 340};
        CHECK(rowLines(node) == expected);
    }
    SECTION("the space tracks size in changing")
    {
        YGNodeStyleSetWidth(node, 400);
        YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
        // The auto column keeps 90; the fr column takes the rest.
        std::vector<float> expected = {0, 310, 400};
        CHECK(columnLines(node) == expected);
    }
    YGNodeFreeRecursive(node);
}

TEST_CASE("Grid virtual contributions copy with their node", "[layout][grid]")
{
    YGNodeRef node = YGNodeNew();
    YGNodeStyleSetDisplay(node, YGDisplayGrid);
    YGNodeStyleSetWidth(node, 100);
    float rows[] = {10, 20};
    YGNodeSetGridVirtualContributions(node, rows, 2, nullptr, 0);
    YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
    YGNodeRef clone = YGNodeClone(node);
    float changed[] = {50, 50};
    YGNodeSetGridVirtualContributions(node, changed, 2, nullptr, 0);
    YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
    // Dirty the clone so it sizes again from its own contributions.
    YGNodeStyleSetWidth(clone, 101);
    YGNodeCalculateLayout(clone, YGUndefined, YGUndefined, YGDirectionLTR);
    CHECK(YGNodeLayoutGetHeight(node) == Approx(100.0f));
    CHECK(YGNodeLayoutGetHeight(clone) == Approx(30.0f));
    YGNodeFree(clone);
    YGNodeFree(node);
}

TEST_CASE("Grid virtual sizes follow a child into the implicit grid before",
          "[layout][grid]")
{
    YGNodeRef node = YGNodeNew();
    YGNodeStyleSetDisplay(node, YGDisplayGrid);
    YGNodeStyleSetWidth(node, 300);
    YGNodeStyleSetHeight(node, 100);
    float columns[] = {100, 10};
    YGNodeSetGridVirtualContributions(node, nullptr, 0, columns, 2);
    YGNodeRef item = YGNodeNew();
    YGNodeStyleSetGridColumnStart(item, 3);
    YGNodeInsertChild(node, item, 0);
    YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
    // Ending at line 1 puts the child in a track before line 1, which shifts
    // the contributions along by one with the track count unchanged.
    YGNodeStyleSetGridColumnStartAuto(item);
    YGNodeStyleSetGridColumnEnd(item, 1);
    YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
    std::vector<float> expected = {0, 0, 100, 110};
    CHECK(columnLines(node) == expected);
    YGNodeFreeRecursive(node);
}

TEST_CASE("Grid virtual contributions that aren't finite count as 0",
          "[layout][grid]")
{
    YGNodeRef node = YGNodeNew();
    YGNodeStyleSetDisplay(node, YGDisplayGrid);
    YGNodeStyleSetWidth(node, 50);
    float rows[] = {NAN, 30};
    REQUIRE(YGNodeSetGridVirtualContributions(node, rows, 2, nullptr, 0));
    CHECK_FALSE(YGNodeSetGridVirtualContributions(node, rows, 2, nullptr, 0));
    YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
    std::vector<float> expected = {0, 0, 30};
    CHECK(rowLines(node) == expected);
    float infinite[] = {INFINITY, 30};
    YGNodeSetGridVirtualContributions(node, infinite, 2, nullptr, 0);
    YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
    CHECK(rowLines(node) == expected);
    CHECK(YGNodeLayoutGetHeight(node) == Approx(30.0f));
    YGNodeFree(node);
}

TEST_CASE("Grid layout records the gap content distribution spaced its rows",
          "[layout][grid]")
{
    YGNodeRef node = YGNodeNew();
    YGNodeStyleSetDisplay(node, YGDisplayGrid);
    YGNodeStyleSetWidth(node, 100);
    YGNodeStyleSetHeight(node, 300);
    YGNodeStyleSetAlignContent(node, YGAlignSpaceBetween);
    YGNodeStyleSetGap(node, YGGutterRow, 10);
    float rows[] = {50, 50};
    YGNodeSetGridVirtualContributions(node, rows, 2, nullptr, 0);
    YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
    // Space between pushes the two rows to either end.
    CHECK(node->getLayout().gridRowGap == Approx(200.0f));
    std::vector<float> expected = {0, 250, 300};
    CHECK(rowLines(node) == expected);
    YGNodeFree(node);
}
