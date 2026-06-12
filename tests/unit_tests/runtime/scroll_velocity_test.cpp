#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout_component.hpp"
#include "utils/no_op_factory.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("ScrollConstraint velocity and scrollActive during drag",
          "[layoutscroll]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    REQUIRE(stateMachine != nullptr);

    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());

    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    REQUIRE(scroll != nullptr);

    artboardInstance->advance(0.0f);

    // Initially idle: velocity is 0, scrollActive is false.
    REQUIRE(scroll->velocityX() == 0.0f);
    REQUIRE(scroll->velocityY() == 0.0f);
    REQUIRE(scroll->scrollActive() == false);

    // Hover over the scroll area.
    smi->pointerMove(rive::Vec2D(50.0f, 250.0f));

    // Pointer down: starts drag.
    smi->pointerDown(rive::Vec2D(50.0f, 250.0f));
    artboardInstance->advance(0.1f);
    smi->advanceAndApply(0.1f);

    // isDragging is now true, so scrollActive should be true even with no
    // movement.
    REQUIRE(scroll->scrollActive() == true);
    REQUIRE(scroll->velocityY() == 0.0f);

    // Drag upward 200px.
    smi->pointerMove(rive::Vec2D(50.0f, 50.0f));
    artboardInstance->advance(0.0f);
    smi->advanceAndApply(0.0f);

    // Velocity should be non-zero after movement.
    REQUIRE(scroll->velocityY() != 0.0f);
    REQUIRE(scroll->scrollActive() == true);

    // Pause: hold still (no pointer move). scrollActive remains true because
    // we're still dragging.
    smi->advanceAndApply(0.1f);
    REQUIRE(scroll->scrollActive() == true);

    // Release: triggers physics.
    smi->pointerUp(rive::Vec2D(50.0f, 50.0f));

    REQUIRE(scroll->physics()->isRunning() == true);
    REQUIRE(scroll->scrollActive() == true);

    delete smi;
}

TEST_CASE("ScrollConstraint velocity resets after physics settles",
          "[layoutscroll]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    REQUIRE(stateMachine != nullptr);

    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());

    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    REQUIRE(scroll != nullptr);

    artboardInstance->advance(0.0f);

    // Swipe: down → move → up.
    smi->pointerMove(rive::Vec2D(50.0f, 250.0f));
    smi->pointerDown(rive::Vec2D(50.0f, 250.0f));
    artboardInstance->advance(0.1f);
    smi->advanceAndApply(0.1f);

    smi->pointerMove(rive::Vec2D(50.0f, 50.0f));
    artboardInstance->advance(0.0f);
    smi->advanceAndApply(0.0f);

    smi->pointerUp(rive::Vec2D(50.0f, 50.0f));

    REQUIRE(scroll->physics()->isRunning() == true);
    REQUIRE(scroll->scrollActive() == true);

    // Advance until physics settles.
    float dt = 0.016f;
    for (int i = 0; i < 600; i++)
    {
        smi->advanceAndApply(dt);
        if (!scroll->physics()->isRunning())
        {
            break;
        }
    }

    REQUIRE(scroll->physics()->isRunning() == false);
    REQUIRE(scroll->scrollActive() == false);
    // After reset, velocity should be zero.
    REQUIRE(scroll->velocityX() == 0.0f);
    REQUIRE(scroll->velocityY() == 0.0f);

    delete smi;
}

TEST_CASE("ScrollConstraint horizontal velocity", "[layoutscroll]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_horizontal.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    REQUIRE(stateMachine != nullptr);

    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());

    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    REQUIRE(scroll != nullptr);

    artboardInstance->advance(0.0f);

    smi->pointerMove(rive::Vec2D(250.0f, 50.0f));
    smi->pointerDown(rive::Vec2D(250.0f, 50.0f));
    artboardInstance->advance(0.1f);
    smi->advanceAndApply(0.1f);

    // Drag left 200px.
    smi->pointerMove(rive::Vec2D(50.0f, 50.0f));
    artboardInstance->advance(0.0f);
    smi->advanceAndApply(0.0f);

    // Horizontal velocity should be non-zero, vertical should be ~0.
    REQUIRE(scroll->velocityX() != 0.0f);
    REQUIRE(scroll->velocityY() == 0.0f);
    REQUIRE(scroll->scrollActive() == true);

    smi->pointerUp(rive::Vec2D(50.0f, 50.0f));

    delete smi;
}

TEST_CASE("ScrollConstraint scrollActive false when idle", "[layoutscroll]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();

    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    REQUIRE(scroll != nullptr);

    artboardInstance->advance(0.0f);

    // Programmatic scroll (no drag, no physics).
    scroll->setScrollPercentY(0.5f);

    REQUIRE(scroll->scrollActive() == false);
    REQUIRE(scroll->velocityX() == 0.0f);
    REQUIRE(scroll->velocityY() == 0.0f);
}
