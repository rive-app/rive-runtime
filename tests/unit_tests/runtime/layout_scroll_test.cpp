#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/elastic_scroll_physics.hpp"
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

// Fixture: viewport 100, content 300 (3x 100-wide items), snap points at item
// leading edges {0, 100, 200}. rangeMin is the maxOffset (negative) and equals
// viewportSize - contentSize - paddingEnd; rangeMax = 0. With friction = 8 and
// speedMultiplier = 1, the helper sets m_speed = accel * 0.00256 inside run(),
// so endTarget = -(value + accel * 0.00032) — pick acceleration to project a
// chosen endTarget.
TEST_CASE("ElasticScrollPhysicsHelper snap respects trailing padding",
          "[layoutscroll]")
{
    const std::vector<float> snaps = {0.0f, 100.0f, 200.0f};
    const float kFriction = 8.0f;
    const float kSpeedMul = 1.0f;
    const float kElastic = 0.66f;
    const float kContent = 300.0f;
    const float kViewport = 100.0f;

    // Settle helper to a stop.
    auto settle = [](rive::ElasticScrollPhysicsHelper& helper) -> float {
        float last = 0.0f;
        for (int i = 0; i < 2000 && helper.isRunning(); i++)
        {
            last = helper.advance(0.016f);
        }
        REQUIRE_FALSE(helper.isRunning());
        return last;
    };

    SECTION("fling past the last item settles at the padded end")
    {
        // paddingRight = 10 → rangeMin = -210. endTarget ≈ 250 (between last
        // item leading edge 200 and the padded end 210, closer to 210).
        rive::ElasticScrollPhysicsHelper helper(kFriction, kSpeedMul, kElastic);
        helper.run(/*acceleration*/ -781250.0f,
                   /*rangeMin*/ -210.0f,
                   /*rangeMax*/ 0.0f,
                   /*value*/ 0.0f,
                   snaps,
                   kContent,
                   kViewport);
        REQUIRE(settle(helper) == Approx(-210.0f).margin(0.5f));
    }

    SECTION("zero trailing padding still snaps to the last item edge")
    {
        // paddingRight = 0 → rangeMin = -200, which equals the last snap point.
        // Behavior should match the pre-fix snap-to-last-item case.
        rive::ElasticScrollPhysicsHelper helper(kFriction, kSpeedMul, kElastic);
        helper.run(/*acceleration*/ -781250.0f,
                   /*rangeMin*/ -200.0f,
                   /*rangeMax*/ 0.0f,
                   /*value*/ 0.0f,
                   snaps,
                   kContent,
                   kViewport);
        REQUIRE(settle(helper) == Approx(-200.0f).margin(0.5f));
    }

    SECTION("mid-content fling snaps to the nearest item, not the padded end")
    {
        // endTarget ≈ 110 → closest snap is 100; padded end 210 is much
        // farther, so per-item snap behavior must be preserved.
        rive::ElasticScrollPhysicsHelper helper(kFriction, kSpeedMul, kElastic);
        helper.run(/*acceleration*/ -343750.0f,
                   /*rangeMin*/ -210.0f,
                   /*rangeMax*/ 0.0f,
                   /*value*/ 0.0f,
                   snaps,
                   kContent,
                   kViewport);
        REQUIRE(settle(helper) == Approx(-100.0f).margin(0.5f));
    }
}

namespace
{
// Swipe up to fling a vertical scroll constraint past its end, then settle.
// Used by the three snap-padding silver tests (one per artboard in the
// fixture). With trailing viewport padding on the parent layout, snap must
// settle at the padded end (rangeMin) rather than at the last item's leading
// edge. Total emitted frames stay around 60: 4 swipe steps + up to 56 settle
// frames (loop exits earlier when physics stops).
void runVerticalSnapPaddingSwipe(const char* artboardName,
                                 const char* silverName)
{
    rive::File::deterministicMode = true;

    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/layout/layout_scroll_snap_padding.riv", &silver);
    REQUIRE(file != nullptr);

    auto artboardSrc = file->artboardNamed(artboardName);
    REQUIRE(artboardSrc != nullptr);
    auto artboard = artboardSrc->instance();
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    // Lists need their default view model bound; static-layout artboards may
    // not ship one. Bind if present.
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    if (viewModelInstance != nullptr)
    {
        artboard->bindViewModelInstance(viewModelInstance);
    }

    auto smi = artboard->defaultStateMachine();
    REQUIRE(smi != nullptr);

    auto renderer = silver.makeRenderer();

    smi->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    auto scrolls = artboard->find<rive::ScrollConstraint>();
    REQUIRE_FALSE(scrolls.empty());
    auto scroll = scrolls[0];
    REQUIRE(scroll != nullptr);
    REQUIRE(scroll->physics() != nullptr);

    float centerX = artboard->width() / 2.0f;
    float startY = artboard->height() / 2.0f;
    float dt = 0.016f;

    // Swipe up: content fills the viewport so a 1500px drag over-shoots even
    // the tallest fixture artboard (~1600px content in a 500px viewport).
    // Whole-second timestamps survive long-long truncation in the physics
    // accumulator under deterministic mode.
    float swipeDistance = 1500.0f;
    float time = 1.0f;

    smi->pointerMove(rive::Vec2D(centerX, startY), time);
    smi->pointerDown(rive::Vec2D(centerX, startY));
    time += 1.0f;

    int steps = 4;
    for (int i = 1; i <= steps; i++)
    {
        silver.addFrame();
        float y = startY - (swipeDistance * i / steps);
        smi->pointerMove(rive::Vec2D(centerX, y), time);
        time += 1.0f;
        smi->advanceAndApply(dt);
        artboard->draw(renderer.get());
    }

    smi->pointerUp(rive::Vec2D(centerX, startY - swipeDistance));

    // Capture frames while physics settles; bail early on settle.
    for (int i = 0; i < 56; i++)
    {
        silver.addFrame();
        smi->advanceAndApply(dt);
        artboard->draw(renderer.get());
        if (!scroll->physics()->isRunning())
        {
            break;
        }
    }

    CHECK(silver.matches(silverName));

    rive::File::deterministicMode = false;
}
} // namespace

TEST_CASE("Scroll snap respects viewport padding (layouts)", "[silver]")
{
    runVerticalSnapPaddingSwipe("ScrollLayouts",
                                "layout_scroll_snap_padding_layouts");
}

TEST_CASE("Scroll snap respects viewport padding (list)", "[silver]")
{
    runVerticalSnapPaddingSwipe("ScrollList",
                                "layout_scroll_snap_padding_list");
}

TEST_CASE("Scroll snap respects viewport padding (virtualized list)",
          "[silver]")
{
    runVerticalSnapPaddingSwipe("ScrollListVirtualized",
                                "layout_scroll_snap_padding_virtualized");
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