#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/text/text.hpp"
#include "utils/no_op_factory.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("ScrollConstraint vertical offset", "[layoutscroll]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("Content") != nullptr);

    REQUIRE(artboard->find<rive::ScrollConstraint>().size() == 1);
    REQUIRE(artboard->find<rive::ScrollConstraint>()[0] != nullptr);
    auto scroll = artboard->find<rive::ScrollConstraint>()[0];

    REQUIRE(scroll->offsetY() == 0);

    artboard->advance(0.0f);
    // scrollPercentY
    scroll->setScrollPercentY(1.0f);
    REQUIRE(scroll->scrollPercentY() == 1.0f);
    REQUIRE(scroll->offsetY() == -610.0f);
    REQUIRE(scroll->clampedOffsetY() == -610.0f);
    REQUIRE(scroll->scrollIndex() == Approx(5.54545f));
    // scrollIndex
    scroll->setScrollIndex(2);
    REQUIRE(scroll->offsetY() == -220.0f);
    REQUIRE(scroll->clampedOffsetY() == -220.0f);
    REQUIRE(scroll->scrollIndex() == 2);

    REQUIRE(scroll->contentHeight() == 1090.0f);
    REQUIRE(scroll->viewportHeight() == 490.0f);
    REQUIRE(scroll->maxOffsetY() == -610.0f);
    REQUIRE(scroll->clampedOffsetY() == -220.0f);
}

TEST_CASE("ScrollConstraint vertical offset manual", "[layoutscroll]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("Content") != nullptr);

    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");

    REQUIRE(artboardInstance != nullptr);
    REQUIRE(artboardInstance->stateMachineCount() == 1);

    REQUIRE(stateMachine != nullptr);

    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());

    REQUIRE(artboardInstance->find<rive::ScrollConstraint>().size() == 1);
    REQUIRE(artboardInstance->find<rive::ScrollConstraint>()[0] != nullptr);
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];

    REQUIRE(scroll->scrollPercentY() == 0.0f);
    REQUIRE(scroll->offsetY() == 0.0f);
    REQUIRE(scroll->scrollIndex() == Approx(0.0f));
    REQUIRE(scroll->physics()->isRunning() == false);

    artboardInstance->advance(0.0f);

    stateMachineInstance->pointerMove(rive::Vec2D(50.0f, 250.0f));
    // Start drag
    stateMachineInstance->pointerDown(rive::Vec2D(50.0f, 250.0f));
    artboardInstance->advance(0.1f);
    stateMachineInstance->advanceAndApply(0.1f);
    // Move up 200px in 0.1 seconds
    stateMachineInstance->pointerMove(rive::Vec2D(50.0f, 50.0f));
    artboardInstance->advance(0.0f);
    stateMachineInstance->advanceAndApply(0.0f);

    REQUIRE(scroll->scrollPercentY() == Approx(0.32787f));
    REQUIRE(scroll->offsetY() == -200.0f);
    REQUIRE(scroll->scrollIndex() == Approx(1.818182f));

    // End drag
    stateMachineInstance->pointerUp(rive::Vec2D(50.0f, 50.0f));

    REQUIRE(scroll->physics()->isRunning() == true);

    delete stateMachineInstance;
}

TEST_CASE("ScrollConstraint horizontal offset", "[layoutscroll]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_horizontal.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::LayoutComponent>("Content") != nullptr);

    REQUIRE(artboard->find<rive::ScrollConstraint>().size() == 1);
    REQUIRE(artboard->find<rive::ScrollConstraint>()[0] != nullptr);
    auto scroll = artboard->find<rive::ScrollConstraint>()[0];

    REQUIRE(scroll->offsetX() == 0);

    artboard->advance(0.0f);
    // scrollPercentX
    scroll->setScrollPercentX(1.0f);
    REQUIRE(scroll->scrollPercentX() == 1.0f);
    REQUIRE(scroll->offsetX() == -610.0f);
    REQUIRE(scroll->clampedOffsetX() == -610.0f);
    REQUIRE(scroll->scrollIndex() == Approx(5.54545f));
    // scrollIndex
    scroll->setScrollIndex(2);
    REQUIRE(scroll->offsetX() == -220.0f);
    REQUIRE(scroll->clampedOffsetX() == -220.0f);
    REQUIRE(scroll->scrollIndex() == 2);

    REQUIRE(scroll->contentWidth() == 1090.0f);
    REQUIRE(scroll->viewportWidth() == 490.0f);
    REQUIRE(scroll->maxOffsetX() == -610.0f);
    REQUIRE(scroll->clampedOffsetX() == -220.0f);
}

TEST_CASE("ScrollConstraint list", "[layoutscroll]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_list.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::LayoutComponent>("Content") != nullptr);
    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);

    REQUIRE(artboard->find<rive::ScrollConstraint>().size() == 1);
    REQUIRE(artboard->find<rive::ScrollConstraint>()[0] != nullptr);
    auto scroll = artboard->find<rive::ScrollConstraint>()[0];
    auto list = artboard->find<rive::ArtboardComponentList>("List");

    REQUIRE(scroll->offsetY() == 0);

    artboard->advance(0.0f);

    REQUIRE(list->numLayoutNodes() == 20);
    for (int i = 0; i < list->numLayoutNodes(); i++)
    {
        auto bounds = list->layoutBoundsForNode(i);
        REQUIRE(bounds.top() == i * 48.0f);
    }

    // scrollIndex
    scroll->setScrollIndex(2);
    REQUIRE(scroll->scrollItemCount() == 20);
    REQUIRE(scroll->offsetY() == -96.0f);
    REQUIRE(scroll->clampedOffsetY() == -96.0f);
    REQUIRE(scroll->scrollIndex() == 2);
    REQUIRE(scroll->contentHeight() == 960.0f);
    REQUIRE(scroll->viewportHeight() == 500.0f);
    REQUIRE(scroll->maxOffsetY() == -460.0f);
}