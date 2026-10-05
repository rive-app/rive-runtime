#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout_component.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <string>

// Virtualizing a list whose layout wraps. The carousel fixture is retyped at
// runtime into a vertical scroll over a wrapping row: 20 items of 100x60, gap
// 10, in a 500 wide content shown through a 200 tall viewport. That's 4 items
// per line and 5 lines 70 apart, 340 tall in all.

namespace
{
struct WrapScroll
{
    rive::rcp<rive::File> file;
    std::unique_ptr<rive::ArtboardInstance> artboard;
    rive::rcp<rive::ViewModelInstance> viewModelInstance;
    rive::ArtboardComponentList* list = nullptr;
    rive::ScrollConstraint* scroll = nullptr;

    explicit WrapScroll(uint8_t widthScale = 1)
    {
        file = ReadRiveFile("assets/component_list_virtualized.riv");
        artboard = file->artboard("Main")->instance();
        viewModelInstance =
            file->createDefaultViewModelInstance(artboard.get());
        artboard->bindViewModelInstance(viewModelInstance);
        list = artboard->find<rive::ArtboardComponentList>("List");
        scroll = artboard->find<rive::ScrollConstraint>()[0];

        auto content = artboard->find<rive::LayoutComponent>("Content");
        // Fill the viewport's width and wrap into lines.
        content->style()->layoutWidthScaleType(widthScale);
        content->style()->flexWrapValue(1);
        scroll->directionValue(1);
        scroll->infinite(false);
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

    // Where item i is drawn, relative to item 0's slot at offset 0.
    rive::Vec2D drawnAt(int i, rive::Vec2D origin)
    {
        auto transform =
            list->worldTransformForArtboard(list->artboardInstance(i));
        return rive::Vec2D(transform[4], transform[5]) - origin;
    }
};
} // namespace

TEST_CASE("Wrapped virtualized list scrolls along the cross axis",
          "[component_list][virtual_layout]")
{
    WrapScroll wrap;
    // Rows wrap, so lines stack vertically even though the main axis is a row.
    REQUIRE(wrap.scroll->virtualAxisIsColumn());
    CHECK(wrap.scroll->contentHeight() == Approx(340.0f));
    // Lines 0..2 reach the 200 tall viewport; line 3 starts at 210.
    CHECK(wrap.realized() == "0 1 2 3 4 5 6 7 8 9 10 11 ");
}

TEST_CASE("Wrapped virtualized list places items on their lines",
          "[component_list][virtual_layout]")
{
    WrapScroll wrap;
    // Bounds are in virtualized content space: line start + slot on the line.
    auto bounds = wrap.list->layoutBoundsForNode(5);
    CHECK(bounds.left() == Approx(110.0f));
    CHECK(bounds.top() == Approx(70.0f));
    CHECK(bounds.width() == Approx(100.0f));
    CHECK(bounds.height() == Approx(60.0f));
    bounds = wrap.list->layoutBoundsForNode(19);
    CHECK(bounds.left() == Approx(330.0f));
    CHECK(bounds.top() == Approx(280.0f));

    // Drawn where the bounds say.
    auto t0 =
        wrap.list->worldTransformForArtboard(wrap.list->artboardInstance(0));
    auto origin = rive::Vec2D(t0[4], t0[5]);
    auto at5 = wrap.drawnAt(5, origin);
    CHECK(at5.x == Approx(110.0f));
    CHECK(at5.y == Approx(70.0f));
    auto at10 = wrap.drawnAt(10, origin);
    CHECK(at10.x == Approx(220.0f));
    CHECK(at10.y == Approx(140.0f));
}

TEST_CASE("Wrapped virtualized list recycles whole lines",
          "[component_list][virtual_layout]")
{
    WrapScroll wrap;
    // Showing 100..300: line 0 has left, lines 1..4 are on screen.
    wrap.scroll->offsetY(-100.0f);
    wrap.settle();
    CHECK(wrap.realized() == "4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 ");
}

TEST_CASE("Wrapped virtualized list buffers lines, not items",
          "[component_list][virtual_layout]")
{
    WrapScroll wrap;
    wrap.scroll->virtualizeBuffer(1);
    wrap.settle();
    // One buffered line after lines 0..2 adds a whole line of items.
    CHECK(wrap.realized() == "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ");
}

TEST_CASE("Hugging wrapped list breaks lines at the viewport's width",
          "[component_list][virtual_layout]")
{
    // Hug sizes the content from its lines, which layout only sees realized;
    // lines break at the space the viewport offers, as they would
    // unvirtualized.
    WrapScroll wrap(2);
    CHECK(wrap.realized() == "0 1 2 3 4 5 6 7 8 9 10 11 ");
    auto bounds = wrap.list->layoutBoundsForNode(5);
    CHECK(bounds.left() == Approx(110.0f));
    CHECK(bounds.top() == Approx(70.0f));
}

TEST_CASE("Wrapped virtualized list scrolled both ways indexes by line",
          "[component_list][virtual_layout]")
{
    WrapScroll wrap;
    wrap.scroll->directionValue(2);
    wrap.settle();
    // Lines still stack vertically; only they are windowed.
    REQUIRE(wrap.scroll->virtualAxisIsColumn());
    CHECK(wrap.realized() == "0 1 2 3 4 5 6 7 8 9 10 11 ");

    // Line 2 (items 8..11) at the top reads as its first item.
    wrap.scroll->scrollOffsetY(-140.0f);
    wrap.settle();
    CHECK(wrap.scroll->scrollIndex() == Approx(8.0f));

    // And an index scrolls to its line.
    wrap.scroll->scrollOffsetY(0.0f);
    wrap.scroll->setScrollIndex(9.0f);
    wrap.settle();
    CHECK(wrap.scroll->offsetY() == Approx(-140.0f));
}

TEST_CASE("Wrapped virtualized list scrolled both ways moves along its lines",
          "[component_list][virtual_layout]")
{
    WrapScroll wrap;
    // Fixed wider than the viewport: 7 items per line, room to scroll across.
    auto content = wrap.artboard->find<rive::LayoutComponent>("Content");
    content->style()->layoutWidthScaleType(0);
    content->width(800.0f);
    wrap.scroll->directionValue(2);
    wrap.settle();
    REQUIRE(wrap.scroll->maxOffsetX() < 0.0f);
    auto before =
        wrap.list->worldTransformForArtboard(wrap.list->artboardInstance(1));

    wrap.scroll->scrollOffsetX(-200.0f);
    wrap.settle();
    auto after =
        wrap.list->worldTransformForArtboard(wrap.list->artboardInstance(1));
    CHECK(after[4] - before[4] == Approx(-200.0f));
    CHECK(after[5] - before[5] == Approx(0.0f));
}

TEST_CASE("Wrapped carousel scrolled both ways loops only its lines",
          "[component_list][virtual_layout]")
{
    WrapScroll wrap;
    // Fixed wider than the viewport, so it also scrolls sideways.
    auto content = wrap.artboard->find<rive::LayoutComponent>("Content");
    content->style()->layoutWidthScaleType(0);
    content->width(800.0f);
    wrap.scroll->directionValue(2);
    wrap.scroll->infinite(true);
    wrap.settle();
    REQUIRE(wrap.scroll->loopsY());
    REQUIRE_FALSE(wrap.scroll->loopsX());
    float maxX = wrap.scroll->maxOffsetX();
    REQUIRE(maxX < 0.0f);
    REQUIRE(maxX > -800.0f);

    // Past either end: lines loop on, the sideways scroll stops at its edge.
    wrap.scroll->scrollBy(rive::Vec2D(-1000.0f, -1000.0f));
    wrap.settle();
    CHECK(wrap.scroll->offsetX() == Approx(maxX));
    CHECK(wrap.scroll->offsetY() == Approx(-1000.0f));

    // At the sideways edge only a vertical delta still moves it.
    CHECK_FALSE(wrap.scroll->canConsume(rive::Vec2D(-50.0f, 0.0f)));
    CHECK(wrap.scroll->canConsume(rive::Vec2D(0.0f, -50.0f)));
}

TEST_CASE("Wrapped virtualized list scrolled only along its lines moves",
          "[component_list][virtual_layout]")
{
    WrapScroll wrap;
    // Lines still stack vertically, but the scroll only moves across them.
    auto content = wrap.artboard->find<rive::LayoutComponent>("Content");
    content->style()->layoutWidthScaleType(0);
    content->width(800.0f);
    wrap.scroll->directionValue(0);
    wrap.settle();
    REQUIRE(wrap.scroll->virtualAxisIsColumn());
    REQUIRE(wrap.scroll->maxOffsetX() < 0.0f);
    auto before =
        wrap.list->worldTransformForArtboard(wrap.list->artboardInstance(1));

    wrap.scroll->scrollOffsetX(-200.0f);
    wrap.settle();
    auto after =
        wrap.list->worldTransformForArtboard(wrap.list->artboardInstance(1));
    CHECK(after[4] - before[4] == Approx(-200.0f));
    CHECK(after[5] - before[5] == Approx(0.0f));
}

TEST_CASE("Virtualization toggled at runtime realizes and recycles items",
          "[component_list][virtual_layout]")
{
    WrapScroll wrap;
    REQUIRE(wrap.realized() == "0 1 2 3 4 5 6 7 8 9 10 11 ");

    // Off: layout takes every item.
    wrap.scroll->virtualize(false);
    wrap.settle();
    CHECK(wrap.realized() == "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 "
                             "19 ");

    // On again: the window keeps what's on screen and recycles the rest.
    wrap.scroll->virtualize(true);
    wrap.settle();
    CHECK(wrap.realized() == "0 1 2 3 4 5 6 7 8 9 10 11 ");
}

TEST_CASE("Hugging wrapped list breaks lines inside its padding and margins",
          "[component_list][virtual_layout]")
{
    // 40 either side leaves 420 of the viewport's 500: three items a line, so
    // item 3 starts the second.
    SECTION("padding")
    {
        WrapScroll wrap(2);
        auto style =
            wrap.artboard->find<rive::LayoutComponent>("Content")->style();
        style->paddingLeft(40.0f);
        style->paddingRight(40.0f);
        wrap.settle();
        auto bounds = wrap.list->layoutBoundsForNode(3);
        CHECK(bounds.left() == Approx(40.0f));
        CHECK(bounds.top() == Approx(70.0f));
    }
    SECTION("margins")
    {
        WrapScroll wrap(2);
        auto style =
            wrap.artboard->find<rive::LayoutComponent>("Content")->style();
        style->marginLeft(40.0f);
        style->marginRight(40.0f);
        style->marginLeftUnitsValue(1);
        style->marginRightUnitsValue(1);
        wrap.settle();
        CHECK(wrap.list->layoutBoundsForNode(3).top() == Approx(70.0f));
    }
}
