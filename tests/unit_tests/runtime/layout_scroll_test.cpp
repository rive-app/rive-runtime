#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/text/text.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/serializing_factory.hpp"
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

TEST_CASE("Carousel snap swipe right settles past index 0", "[silver]")
{
    rive::File::deterministicMode = true;

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/layout/layout_scroll_snap.riv", &silver);

    auto artboard = file->artboard("main")->instance();
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    auto smi = artboard->defaultStateMachine();
    REQUIRE(smi != nullptr);

    auto renderer = silver.makeRenderer();

    smi->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    float centerY = artboard->height() / 2.0f;
    float startX = artboard->width() / 2.0f;
    float dt = 0.016f;

    // Swipe right: use whole-second timestamps so they survive long long
    // truncation in the physics accumulator.
    float swipeDistance = 1500.0f;
    float time = 1.0f;

    smi->pointerMove(rive::Vec2D(startX, centerY), time);
    smi->pointerDown(rive::Vec2D(startX, centerY));
    time += 1.0f;

    int steps = 4;
    for (int i = 1; i <= steps; i++)
    {
        silver.addFrame();
        float x = startX + (swipeDistance * i / steps);
        smi->pointerMove(rive::Vec2D(x, centerY), time);
        time += 1.0f;
        smi->advanceAndApply(dt);
        artboard->draw(renderer.get());
    }

    smi->pointerUp(rive::Vec2D(startX + swipeDistance, centerY));

    // Capture frames while physics settles.
    for (int i = 0; i < 300; i++)
    {
        silver.addFrame();
        smi->advanceAndApply(dt);
        artboard->draw(renderer.get());
        if (!smi->artboard()
                 ->find<rive::ScrollConstraint>()[0]
                 ->physics()
                 ->isRunning())
        {
            break;
        }
    }

    CHECK(silver.matches("layout_scroll_snap_carousel"));

    rive::File::deterministicMode = false;
}

TEST_CASE("ScrollConstraint nearestSnapOffsetInDirection", "[layoutscroll]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard()->instance();
    artboard->advance(0.0f);

    REQUIRE(artboard->find<rive::ScrollConstraint>().size() == 1);
    REQUIRE(artboard->find<rive::ScrollConstraint>()[0] != nullptr);
    auto scroll = artboard->find<rive::ScrollConstraint>()[0];

    // Confirm fixture: index→offset step is 110, so snap offsets are
    // 0, -110, -220, -330, -440, -550.
    scroll->setScrollIndex(2);
    REQUIRE(scroll->offsetY() == -220.0f);

    // Snap disabled: target returned unchanged on both axes.
    REQUIRE(scroll->snap() == false);
    rive::Vec2D passthrough =
        scroll->nearestSnapOffsetInDirection(rive::Vec2D(0.0f, 0.0f),
                                             rive::Vec2D(42.0f, -150.0f));
    REQUIRE(passthrough.x == 42.0f);
    REQUIRE(passthrough.y == -150.0f);

    scroll->snap(true);

    // Scrolling forward (target more negative): pick closest snap ≤ target.
    // target=-150 → candidates {-220,-330,-440,-550} → nearest is -220.
    rive::Vec2D forward =
        scroll->nearestSnapOffsetInDirection(rive::Vec2D(0.0f, 0.0f),
                                             rive::Vec2D(0.0f, -150.0f));
    REQUIRE(forward.y == -220.0f);

    // Scrolling back (target less negative): pick closest snap ≥ target.
    // target=-150 → candidates {0,-110} → nearest is -110.
    rive::Vec2D backward =
        scroll->nearestSnapOffsetInDirection(rive::Vec2D(0.0f, -500.0f),
                                             rive::Vec2D(0.0f, -150.0f));
    REQUIRE(backward.y == -110.0f);

    // current == target: returned unchanged (no direction to search).
    rive::Vec2D noop =
        scroll->nearestSnapOffsetInDirection(rive::Vec2D(0.0f, -330.0f),
                                             rive::Vec2D(0.0f, -330.0f));
    REQUIRE(noop.y == -330.0f);

    // Target already on a snap: stays on the same snap.
    rive::Vec2D onSnap =
        scroll->nearestSnapOffsetInDirection(rive::Vec2D(0.0f, 0.0f),
                                             rive::Vec2D(0.0f, -220.0f));
    REQUIRE(onSnap.y == -220.0f);
}

TEST_CASE("ScrollConstraint scrollIndex with hidden items", "[silver]")
{
    rive::File::deterministicMode = true;

    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/layout/layout_scroll_visibility.riv", &silver);

    auto artboard = file->artboard()->instance();
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);
    artboard->bindViewModelInstance(vmi);

    auto smi = artboard->defaultStateMachine();
    REQUIRE(smi != nullptr);

    auto vis2 = vmi->propertyValue("vis2")->as<rive::ViewModelInstanceEnum>();
    auto vis3 = vmi->propertyValue("vis3")->as<rive::ViewModelInstanceEnum>();
    auto vis4 = vmi->propertyValue("vis4")->as<rive::ViewModelInstanceEnum>();

    auto renderer = silver.makeRenderer();
    float dt = 1.0f / 60.0f;

    smi->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    // Run 300 frames, toggling visibility at key moments.
    for (int frame = 0; frame < 300; frame++)
    {
        // Hide item 2 at frame 30.
        if (frame == 30)
        {
            vis2->value(1);
        }
        // Hide item 3 at frame 90.
        if (frame == 90)
        {
            vis3->value(1);
        }
        // Show item 2 again at frame 150.
        if (frame == 150)
        {
            vis2->value(0);
        }
        // Hide item 4 at frame 210.
        if (frame == 210)
        {
            vis4->value(1);
        }
        // Show all at frame 270.
        if (frame == 270)
        {
            vis2->value(0);
            vis3->value(0);
            vis4->value(0);
        }

        silver.addFrame();
        smi->advanceAndApply(dt);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("layout_scroll_visibility"));

    rive::File::deterministicMode = false;
}