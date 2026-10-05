#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout/grid_track.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout_component.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <string>

// Virtualizing a list in a grid. layout_scroll_grid_virtualized.riv is the
// virtualized carousel made a vertical scroll over a grid: 20 items of 100x60
// in four 100px columns with 10px gaps, the first row a fixed 80px and the
// rest sized to their items, shown through a 200 tall viewport. Rows land at
// 0, 90, 160, 230 and 300; 360 tall in all. Center alignment centers items in
// the 80px row.

namespace
{
struct GridScroll
{
    rive::rcp<rive::File> file;
    std::unique_ptr<rive::ArtboardInstance> artboard;
    rive::rcp<rive::ViewModelInstance> viewModelInstance;
    rive::ArtboardComponentList* list = nullptr;
    rive::ScrollConstraint* scroll = nullptr;

    GridScroll()
    {
        file = ReadRiveFile("assets/layout/layout_scroll_grid_virtualized.riv");
        artboard = file->artboard("Main")->instance();
        viewModelInstance =
            file->createDefaultViewModelInstance(artboard.get());
        artboard->bindViewModelInstance(viewModelInstance);
        list = artboard->find<rive::ArtboardComponentList>("List");
        scroll = artboard->find<rive::ScrollConstraint>()[0];
        settle();
    }

    // Layout, then the window it allows, then the items it realized.
    void settle()
    {
        for (int i = 0; i < 3; i++)
        {
            artboard->advance(0.0f);
        }
    }

    std::string realized()
    {
        std::string out;
        for (int i = 0; i < list->artboardCount(); i++)
        {
            if (list->artboardInstance(i) != nullptr)
            {
                out += std::to_string(i) + " ";
            }
        }
        return out;
    }

    rive::Vec2D drawnAt(int i)
    {
        auto transform =
            list->worldTransformForArtboard(list->artboardInstance(i));
        return rive::Vec2D(transform[4], transform[5]);
    }
};
} // namespace

TEST_CASE("Grid virtualized list windows rows",
          "[component_list][virtual_layout]")
{
    GridScroll grid;
    REQUIRE(grid.scroll->virtualizesGrid());
    REQUIRE(grid.scroll->virtualAxisIsColumn());
    CHECK(grid.scroll->contentHeight() == Approx(360.0f));
    // Rows 0..2 reach the 200 tall viewport; row 3 starts at 230.
    CHECK(grid.realized() == "0 1 2 3 4 5 6 7 8 9 10 11 ");
}

TEST_CASE("Grid virtualized list places items in their cells",
          "[component_list][virtual_layout]")
{
    GridScroll grid;
    // Bounds: the column start layout resolved, the row start we stack.
    auto bounds = grid.list->layoutBoundsForNode(1);
    CHECK(bounds.left() == Approx(110.0f));
    // Centered in the fixed 80px first row.
    CHECK(bounds.top() == Approx(10.0f));
    bounds = grid.list->layoutBoundsForNode(19);
    CHECK(bounds.left() == Approx(330.0f));
    CHECK(bounds.top() == Approx(300.0f));

    // Drawn in the same cells, relative to item 0.
    auto origin = grid.drawnAt(0);
    auto at1 = grid.drawnAt(1) - origin;
    CHECK(at1.x == Approx(110.0f));
    CHECK(at1.y == Approx(0.0f));
    auto at5 = grid.drawnAt(5) - origin;
    CHECK(at5.x == Approx(110.0f));
    CHECK(at5.y == Approx(80.0f));
}

TEST_CASE("Grid virtualized list recycles whole rows",
          "[component_list][virtual_layout]")
{
    GridScroll grid;
    // Showing 100..300: row 0 has left, row 4 starts exactly at the far edge.
    grid.scroll->offsetY(-100.0f);
    grid.settle();
    CHECK(grid.realized() == "4 5 6 7 8 9 10 11 12 13 14 15 ");

    // A realized row starts at column 0, so items keep their columns.
    auto origin = grid.drawnAt(4);
    CHECK((grid.drawnAt(6) - origin).x == Approx(220.0f));
    CHECK((grid.drawnAt(8) - origin).y == Approx(70.0f));
}

TEST_CASE("Grid virtualization needs a vertical scroll",
          "[component_list][virtual_layout]")
{
    GridScroll grid;
    grid.scroll->directionValue(0);
    CHECK_FALSE(grid.scroll->virtualizesGrid());
}

TEST_CASE("Grid scrolled vertically windows columns it can't show",
          "[component_list][virtual_layout]")
{
    GridScroll grid;
    // A 250 wide viewport over the 430 wide grid, scrolled vertically only.
    grid.artboard->width(250.0f);
    grid.artboard->find<rive::LayoutComponent>("Content")
        ->style()
        ->layoutWidthScaleType(2);
    grid.settle();
    REQUIRE(grid.scroll->virtualizesGridColumns());
    REQUIRE_FALSE(grid.scroll->indexesGridCells());
    // Rows 0..2 by columns 0..2: column 3 starts at 330.
    CHECK(grid.realized() == "0 1 2 4 5 6 8 9 10 ");
    auto origin = grid.drawnAt(0);
    CHECK((grid.drawnAt(2) - origin).x == Approx(220.0f));
    CHECK((grid.drawnAt(4) - origin).y == Approx(80.0f));

    // Rows still recycle, keeping the same columns.
    grid.scroll->scrollOffsetY(-100.0f);
    grid.settle();
    CHECK(grid.realized() == "4 5 6 8 9 10 12 13 14 ");
    // The index still reads along the rows, fraction and all.
    grid.scroll->scrollOffsetY(-45.0f);
    grid.settle();
    CHECK(grid.scroll->scrollIndex() == Approx(0.5f));
}

TEST_CASE("Grid columns window from where the content sits",
          "[component_list][virtual_layout]")
{
    GridScroll grid;
    // The 430 wide grid centered in a 250 wide viewport hangs 90 past either
    // side, so the viewport shows 90..340 of it: all four columns, though
    // only three fit from 0.
    grid.artboard->width(250.0f);
    grid.artboard->find<rive::LayoutComponent>("Content")
        ->style()
        ->layoutWidthScaleType(2);
    grid.scroll->viewport()->style()->layoutAlignmentType(4);
    grid.settle();
    REQUIRE(grid.scroll->virtualizesGridColumns());
    REQUIRE(grid.scroll->content()->layoutX() == Approx(-90.0f));
    CHECK(grid.list->artboardInstance(0) != nullptr);
    CHECK(grid.list->artboardInstance(3) != nullptr);
    CHECK((grid.drawnAt(3) - grid.drawnAt(0)).x == Approx(330.0f));
}

TEST_CASE("Grid virtualization turned off unpins its cells",
          "[component_list][virtual_layout]")
{
    GridScroll grid;
    // Rows 1..3 realized: item 4 is pinned to the first realized row.
    grid.scroll->offsetY(-100.0f);
    grid.settle();
    REQUIRE(grid.realized() == "4 5 6 7 8 9 10 11 12 13 14 15 ");

    // Off: every item auto-places into its own cell again.
    grid.scroll->virtualize(false);
    grid.settle();
    auto origin = grid.drawnAt(0);
    auto at4 = grid.drawnAt(4) - origin;
    CHECK(at4.x == Approx(0.0f));
    CHECK(at4.y == Approx(80.0f));
    auto at19 = grid.drawnAt(19) - origin;
    CHECK(at19.x == Approx(330.0f));
    CHECK(at19.y == Approx(290.0f));
}

TEST_CASE("Grid scrolled both ways windows rows and columns",
          "[component_list][virtual_layout]")
{
    GridScroll grid;
    // A 250 wide viewport over the 430 wide grid (content hugs its columns),
    // scrolled both ways.
    grid.artboard->width(250.0f);
    grid.artboard->find<rive::LayoutComponent>("Content")
        ->style()
        ->layoutWidthScaleType(2);
    grid.scroll->directionValue(2);
    grid.settle();
    REQUIRE(grid.scroll->virtualizesGridColumns());
    CHECK(grid.scroll->contentWidth() == Approx(430.0f));
    // Rows 0..2 by columns 0..2: column 3 starts at 330.
    CHECK(grid.realized() == "0 1 2 4 5 6 8 9 10 ");

    // Showing 150..400 across: column 0 has left, column 3 is in.
    grid.scroll->offsetX(-150.0f);
    grid.settle();
    CHECK(grid.realized() == "1 2 3 5 6 7 9 10 11 ");
    auto origin = grid.drawnAt(1);
    CHECK((grid.drawnAt(3) - origin).x == Approx(220.0f));
    CHECK((grid.drawnAt(5) - origin).y == Approx(80.0f));
    // Item 1 sits at its column start, moved by the horizontal scroll.
    auto content = grid.artboard->find<rive::LayoutComponent>("Content");
    CHECK(origin.x - content->worldTransform()[4] == Approx(110.0f - 150.0f));
}

TEST_CASE("Grid scrolled across anchors to an item on screen",
          "[component_list][virtual_layout]")
{
    GridScroll grid;
    grid.artboard->width(250.0f);
    grid.artboard->find<rive::LayoutComponent>("Content")
        ->style()
        ->layoutWidthScaleType(2);
    grid.scroll->directionValue(2);
    grid.settle();
    // 150 across: column 0 has left, so item 1 is the first on screen. A
    // little down too, since a scroll at its start doesn't anchor.
    grid.scroll->scrollOffsetX(-150.0f);
    grid.scroll->scrollOffsetY(-20.0f);
    grid.settle();
    REQUIRE(grid.realized() == "1 2 3 5 6 7 9 10 11 ");

    rive::ViewModelInstanceList* items = nullptr;
    for (auto& value : grid.viewModelInstance->propertyValues())
    {
        if (value->is<rive::ViewModelInstanceList>())
        {
            items = value->as<rive::ViewModelInstanceList>();
            break;
        }
    }
    REQUIRE(items != nullptr);
    // Swapping items 1 and 5 moves item 1 a row down, from 0 to 90: the
    // scroll follows it rather than whatever is now at its old index.
    items->swap(1, 5);
    grid.settle();
    CHECK(grid.scroll->offsetY() == Approx(-110.0f));
}

TEST_CASE("Grid carousel scrolled both ways cycles rows and columns",
          "[component_list][virtual_layout]")
{
    GridScroll grid;
    // A 250 wide viewport over the 430 wide grid, cycling both ways: columns
    // repeat every 440, rows every 370.
    grid.artboard->width(250.0f);
    grid.artboard->find<rive::LayoutComponent>("Content")
        ->style()
        ->layoutWidthScaleType(2);
    grid.scroll->directionValue(2);
    grid.scroll->infinite(true);
    grid.settle();
    REQUIRE(grid.scroll->loopsX());
    REQUIRE(grid.scroll->loopsY());
    CHECK(grid.scroll->contentWidth() == Approx(440.0f));
    CHECK(grid.realized() == "0 1 2 4 5 6 8 9 10 ");

    // Showing 330..580 across: column 3, then columns 0 and 1 again.
    grid.scroll->scrollOffsetX(-330.0f);
    grid.settle();
    CHECK(grid.realized() == "0 1 3 4 5 7 8 9 11 ");
    // Items paint in on screen order.
    CHECK(grid.list->orderedListIndices() ==
          std::vector<int>{3, 0, 1, 7, 4, 5, 11, 8, 9});
    // Column 0's copy sits a cycle on, right after column 3.
    CHECK((grid.drawnAt(0) - grid.drawnAt(3)).x == Approx(110.0f));
    CHECK((grid.drawnAt(1) - grid.drawnAt(0)).x == Approx(110.0f));
    CHECK((grid.drawnAt(4) - grid.drawnAt(0)).y == Approx(80.0f));

    // A cycle down and row 1 on: rows 1..3 show, and the index reads the
    // cell at the top left.
    grid.scroll->scrollOffsetY(-460.0f);
    grid.settle();
    CHECK(grid.realized() == "4 5 7 8 9 11 12 13 15 ");
    CHECK(grid.scroll->scrollIndex() == Approx(7.0f));
}

TEST_CASE("Grid auto columns size from items that aren't realized",
          "[component_list][virtual_layout]")
{
    GridScroll grid;
    // Make the first column auto, then give item 16 (row 4, off screen) a
    // 150 wide measurement. Layout only sees rows 0..2, all 100 wide, so on
    // its own it would size the column to 100.
    for (auto track : grid.artboard->find<rive::GridTrack>())
    {
        if (track->gridCollection() ==
            rive::GridTrackCollection::templateColumns)
        {
            track->trackType((uint32_t)rive::GridTrackSizeType::autoSize);
            break;
        }
    }
    grid.list->setItemSize(rive::Vec2D(150.0f, 60.0f), 16);
    grid.settle();
    // Column 1 starts after a 150 wide column 0 and its gap.
    CHECK(grid.list->layoutBoundsForNode(1).left() == Approx(160.0f));
    CHECK((grid.drawnAt(1) - grid.drawnAt(0)).x == Approx(160.0f));
}
