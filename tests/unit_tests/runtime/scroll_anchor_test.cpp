#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>

// Scroll anchoring: when an item before the first one on screen measures
// differently from its authored size, the offset moves with it so what's on
// screen stays put. Uses the virtualized list as a plain horizontal list: 20
// items 100 wide with 10px gaps, item k starting at 110k.

namespace
{
struct AnchorScroll
{
    rive::rcp<rive::File> file;
    std::unique_ptr<rive::ArtboardInstance> artboard;
    rive::rcp<rive::ViewModelInstance> viewModelInstance;
    rive::ArtboardComponentList* list = nullptr;
    rive::ScrollConstraint* scroll = nullptr;

    explicit AnchorScroll(float offset)
    {
        file = ReadRiveFile("assets/component_list_virtualized.riv");
        artboard = file->artboard("Main")->instance();
        viewModelInstance =
            file->createDefaultViewModelInstance(artboard.get());
        artboard->bindViewModelInstance(viewModelInstance);
        list = artboard->find<rive::ArtboardComponentList>("List");
        scroll = artboard->find<rive::ScrollConstraint>()[0];
        scroll->infinite(false);
        scroll->offsetX(offset);
        for (int i = 0; i < 3; i++)
        {
            artboard->advance(0.0f);
        }
    }

    // As if item index measured at width once laid out.
    void measure(int index, float width)
    {
        list->setItemSize(rive::Vec2D(width, 60.0f), index);
        scroll->constrainVirtualized(true);
    }
};
} // namespace

TEST_CASE("Scroll anchoring keeps the first item on screen in place",
          "[virtual_layout]")
{
    // At 500, item 4 (440..540) is the first on screen.
    AnchorScroll anchor(-500.0f);
    REQUIRE(anchor.scroll->offsetX() == Approx(-500.0f));
    // Item 1 is off screen before it; growing by 50 pushes item 4 to 490.
    anchor.measure(1, 150.0f);
    CHECK(anchor.scroll->offsetX() == Approx(-550.0f));
}

TEST_CASE("Scroll anchoring ignores items after the first on screen",
          "[virtual_layout]")
{
    AnchorScroll anchor(-500.0f);
    anchor.measure(12, 150.0f);
    CHECK(anchor.scroll->offsetX() == Approx(-500.0f));
}

TEST_CASE("Scroll anchoring lets content grow below at the start",
          "[virtual_layout]")
{
    AnchorScroll anchor(0.0f);
    anchor.measure(1, 150.0f);
    CHECK(anchor.scroll->offsetX() == Approx(0.0f));
}
