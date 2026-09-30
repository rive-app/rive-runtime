#include "rive/animation/state_machine_instance.hpp"
#include "rive/constraints/scrolling/clamped_scroll_physics.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/constraints/scrolling/scroll_physics.hpp"
#include "rive/layout_component.hpp"
#include "rive/scroll_event.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <cstdio>
#include <memory>

// Indirect scroll input: wheel and trackpad. Direct touch stays on the pointer
// path and is covered by layout_scroll_test / scroll_velocity_test.
//
// assets/layout/layout_scroll_vertical.riv scrolls y over [-610, 0].
// assets/layout/layout_scroll_horizontal.riv scrolls x over [-610, 0].

static rive::ScrollEvent wheel(float dx, float dy)
{
    rive::ScrollEvent event;
    event.delta = rive::Vec2D(dx, dy);
    event.phase = rive::ScrollPhase::update;
    event.precise = false;
    return event;
}

static rive::ScrollEvent trackpad(rive::ScrollPhase phase, float dx, float dy)
{
    rive::ScrollEvent event;
    event.delta = rive::Vec2D(dx, dy);
    event.phase = phase;
    event.precise = true;
    return event;
}

// Long enough to trip the constraint's idle timer, which closes a gesture that
// reports no end of its own.
static void advancePastIdle(rive::StateMachineInstance* smi)
{
    smi->advanceAndApply(0.2f);
}

TEST_CASE("Wheel scrolls without flinging", "[scrollinput]")
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
    smi->advanceAndApply(0.0f);

    REQUIRE(scroll->offsetY() == 0.0f);

    auto hit = smi->pointerScroll(rive::Vec2D(50.0f, 250.0f), wheel(0, -50.0f));
    REQUIRE(hit != rive::HitResult::none);
    REQUIRE(scroll->offsetY() == -50.0f);

    smi->pointerScroll(rive::Vec2D(50.0f, 250.0f), wheel(0, -70.0f));
    REQUIRE(scroll->offsetY() == -120.0f);

    // A detent never accumulates velocity, so nothing can fling from it.
    REQUIRE(scroll->velocityY() == 0.0f);
    REQUIRE(scroll->physics()->isRunning() == false);

    delete smi;
}

TEST_CASE("Wheel reports the view as actively scrolling until it goes idle",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    REQUIRE(scroll->scrollActive() == false);

    smi->pointerScroll(rive::Vec2D(50.0f, 250.0f), wheel(0, -50.0f));
    REQUIRE(scroll->scrollActive() == true);

    // Still inside the idle window.
    smi->advanceAndApply(0.05f);
    REQUIRE(scroll->scrollActive() == true);

    advancePastIdle(smi);
    REQUIRE(scroll->scrollActive() == false);

    delete smi;
}

TEST_CASE("A view that cannot move declines the scroll", "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    // Already at the top: a delta that would scroll back past it is declined,
    // which is what lets a gesture chain out to an enclosing view.
    REQUIRE(scroll->canConsume(rive::Vec2D(0, 50.0f)) == false);
    auto hit = smi->pointerScroll(rive::Vec2D(50.0f, 250.0f), wheel(0, 50.0f));
    REQUIRE(hit == rive::HitResult::none);
    REQUIRE(scroll->offsetY() == 0.0f);
    REQUIRE(scroll->scrollActive() == false);

    // The other direction has room.
    REQUIRE(scroll->canConsume(rive::Vec2D(0, -50.0f)) == true);
    REQUIRE(smi->pointerScroll(rive::Vec2D(50.0f, 250.0f), wheel(0, -50.0f)) !=
            rive::HitResult::none);

    delete smi;
}

TEST_CASE("Scroll outside the viewport is not consumed", "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    auto hit =
        smi->pointerScroll(rive::Vec2D(-500.0f, -500.0f), wheel(0, -50.0f));
    REQUIRE(hit == rive::HitResult::none);
    REQUIRE(scroll->offsetY() == 0.0f);

    delete smi;
}

TEST_CASE("A gesture stays latched to its target past the edge",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    rive::Vec2D over(50.0f, 250.0f);

    // Run down to the end of the range.
    REQUIRE(smi->pointerScroll(over, wheel(0, -600.0f)) !=
            rive::HitResult::none);
    REQUIRE(smi->pointerScroll(over, wheel(0, -100.0f)) !=
            rive::HitResult::none);
    REQUIRE(scroll->offsetY() == -610.0f);
    REQUIRE(scroll->canConsume(rive::Vec2D(0, -100.0f)) == false);

    // Pinned at the edge but still mid-gesture: the latch keeps the event here
    // rather than handing the tail of the flick to whatever encloses it.
    REQUIRE(smi->hasScrollLatch() == true);
    REQUIRE(smi->pointerScroll(over, wheel(0, -100.0f)) !=
            rive::HitResult::none);

    // Once the gesture goes idle the latch is released, and a view that can't
    // move declines again.
    advancePastIdle(smi);
    REQUIRE(smi->hasScrollLatch() == false);
    REQUIRE(smi->pointerScroll(over, wheel(0, -100.0f)) ==
            rive::HitResult::none);

    delete smi;
}

TEST_CASE("Trackpad flings on release", "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    rive::Vec2D over(50.0f, 250.0f);

    // A zero-delta begin claims nothing; the first directional update opens
    // the gesture.
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::begin, 0, 0));
    REQUIRE(scroll->scrollActive() == false);

    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -40.0f));
    REQUIRE(scroll->scrollActive() == true);
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -40.0f));
    REQUIRE(scroll->offsetY() == -80.0f);

    // Unlike a wheel, a precise gesture accumulates velocity to fling from.
    // The opening delta primes the clock, so the second one sets the speed.
    REQUIRE(scroll->velocityY() != 0.0f);

    smi->pointerScroll(over, trackpad(rive::ScrollPhase::end, 0, 0));
    REQUIRE(scroll->physics()->isRunning() == true);

    delete smi;
}

TEST_CASE("Platform momentum never starts a fling", "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    rive::Vec2D over(50.0f, 250.0f);

    // A host whose platform generates its own momentum sends the tail as
    // momentum deltas. They land directly; flinging on top would double them.
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::momentum, 0, -30.0f));
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::momentum, 0, -20.0f));

    REQUIRE(scroll->offsetY() == -50.0f);
    REQUIRE(scroll->physics()->isRunning() == false);
    REQUIRE(scroll->velocityY() == 0.0f);

    delete smi;
}

TEST_CASE("Inertia cancel halts a running fling", "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    rive::Vec2D over(50.0f, 250.0f);

    smi->pointerScroll(over, trackpad(rive::ScrollPhase::begin, 0, 0));
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -40.0f));
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::end, 0, 0));
    REQUIRE(scroll->physics()->isRunning() == true);

    // Fingers back down on the trackpad while the content is still coasting.
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::inertiaCancel, 0, 0));
    REQUIRE(scroll->physics()->isRunning() == false);
    REQUIRE(scroll->velocityY() == 0.0f);

    delete smi;
}

TEST_CASE("Scroll only moves the axes the view constrains", "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_horizontal.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    REQUIRE(stateMachine != nullptr);
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    rive::Vec2D over(artboardInstance->width() / 2.0f,
                     artboardInstance->height() / 2.0f);

    // Vertical delta over a horizontal view: nothing to move, so it declines.
    REQUIRE(scroll->canConsume(rive::Vec2D(0, -50.0f)) == false);
    REQUIRE(smi->pointerScroll(over, wheel(0, -50.0f)) ==
            rive::HitResult::none);

    REQUIRE(smi->pointerScroll(over, wheel(-50.0f, 0)) !=
            rive::HitResult::none);
    REQUIRE(scroll->offsetX() == -50.0f);
    REQUIRE(scroll->offsetY() == 0.0f);

    delete smi;
}

TEST_CASE("A snapping view settles when a wheel gesture goes idle",
          "[scrollinput]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/layout/layout_scroll_snap.riv", &silver);
    auto artboard = file->artboard("main")->instance();
    REQUIRE(artboard != nullptr);

    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    if (viewModelInstance != nullptr)
    {
        artboard->bindViewModelInstance(viewModelInstance);
    }

    auto smi = artboard->defaultStateMachine();
    REQUIRE(smi != nullptr);
    smi->advanceAndApply(0.0f);

    auto scroll = artboard->find<rive::ScrollConstraint>()[0];
    REQUIRE(scroll != nullptr);
    REQUIRE(scroll->snap() == true);

    rive::Vec2D over(artboard->width() / 2.0f, artboard->height() / 2.0f);
    REQUIRE(smi->pointerScroll(over, wheel(-30.0f, 0)) !=
            rive::HitResult::none);

    // No fling from a detent, so nothing is running yet.
    REQUIRE(scroll->physics()->isRunning() == false);

    // Going idle is the only end a wheel has; a snapping view settles there.
    // Stepped in frames rather than one long advance so the settle is
    // caught starting, not already finished.
    bool settling = false;
    for (int i = 0; i < 20 && !settling; i++)
    {
        smi->advanceAndApply(0.016f);
        settling = scroll->physics()->isRunning();
    }
    REQUIRE(settling == true);
}

TEST_CASE("A non-snapping view just stops when a wheel gesture goes idle",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);
    REQUIRE(scroll->snap() == false);

    smi->pointerScroll(rive::Vec2D(50.0f, 250.0f), wheel(0, -50.0f));
    advancePastIdle(smi);

    REQUIRE(scroll->offsetY() == -50.0f);
    REQUIRE(scroll->physics()->isRunning() == false);
    REQUIRE(scroll->scrollActive() == false);

    delete smi;
}

// assets/layout/scroll_nested.riv is built from the scroll_demo CLI sample
// (packages/rive-cli/samples/scroll_demo): a vertical list with a horizontal
// strip as its fourth row. The hit point and the extents are read back from
// the resolved layout rather than hard coded.

struct NestedScene
{
    std::unique_ptr<rive::ArtboardInstance> artboard;
    rive::StateMachineInstance* smi = nullptr;
    rive::ScrollConstraint* outer = nullptr;
    rive::ScrollConstraint* strip = nullptr;
    rive::Vec2D overStrip;
};

static void openNested(rive::File* file, NestedScene& scene)
{
    auto artboard = file->artboard();
    scene.artboard = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    REQUIRE(stateMachine != nullptr);
    scene.smi =
        new rive::StateMachineInstance(stateMachine, scene.artboard.get());
    scene.outer = scene.artboard->find<rive::ScrollConstraint>("Scroll V");
    scene.strip = scene.artboard->find<rive::ScrollConstraint>("Scroll H");
    REQUIRE(scene.outer != nullptr);
    REQUIRE(scene.strip != nullptr);
    scene.smi->advanceAndApply(0.0f);

    auto stripViewport =
        scene.artboard->find<rive::LayoutComponent>("Strip Viewport");
    REQUIRE(stripViewport != nullptr);
    scene.overStrip = stripViewport->worldBounds().center();
}

TEST_CASE("A scroll the inner view cannot use chains to the outer one",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/scroll_nested.riv");
    NestedScene scene;
    openNested(file.get(), scene);

    // The strip is topmost under this point but only constrains x, so a
    // vertical scroll falls through to the list behind it.
    REQUIRE(scene.smi->pointerScroll(scene.overStrip, wheel(0, -50.0f)) !=
            rive::HitResult::none);
    REQUIRE(scene.outer->offsetY() == -50.0f);
    REQUIRE(scene.strip->offsetX() == 0.0f);

    delete scene.smi;
}

TEST_CASE("The inner view takes a scroll on the axis it owns", "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/scroll_nested.riv");
    NestedScene scene;
    openNested(file.get(), scene);

    REQUIRE(scene.smi->pointerScroll(scene.overStrip, wheel(-50.0f, 0)) !=
            rive::HitResult::none);
    REQUIRE(scene.strip->offsetX() == -50.0f);
    REQUIRE(scene.outer->offsetY() == 0.0f);

    delete scene.smi;
}

TEST_CASE("A latched inner view does not hand its tail to the outer one",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/scroll_nested.riv");
    NestedScene scene;
    openNested(file.get(), scene);

    // Run the strip to its end.
    scene.smi->pointerScroll(scene.overStrip, wheel(-700.0f, 0));
    REQUIRE(scene.strip->offsetX() == scene.strip->maxOffsetX());
    REQUIRE(scene.strip->canConsume(rive::Vec2D(-100.0f, 0)) == false);

    // Still the same gesture, now carrying a vertical component the strip
    // could never use. Without the latch this is where the outer list would
    // grab the rest of the flick.
    scene.smi->pointerScroll(scene.overStrip, wheel(-100.0f, -100.0f));
    REQUIRE(scene.outer->offsetY() == 0.0f);
    REQUIRE(scene.strip->offsetX() == scene.strip->maxOffsetX());

    // Idle releases the latch, and the next gesture chains normally again.
    advancePastIdle(scene.smi);
    REQUIRE(scene.smi->hasScrollLatch() == false);
    scene.smi->pointerScroll(scene.overStrip, wheel(0, -50.0f));
    REQUIRE(scene.outer->offsetY() == -50.0f);

    delete scene.smi;
}

TEST_CASE("A gesture that reports no end closes when it goes idle",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    rive::Vec2D over(50.0f, 250.0f);

    // A platform that owns its own momentum (macOS) sends begin and updates
    // but never an end, so runPhysics never runs to clear what initPhysics
    // set. Only the idle timer closes the gesture.
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::begin, 0, 0));
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -40.0f));
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -40.0f));
    REQUIRE(scroll->scrollActive() == true);

    advancePastIdle(smi);

    // scrollActive covers the dragging flag, so this failing means the
    // artboard would keep asking to advance forever.
    REQUIRE(scroll->scrollActive() == false);
    REQUIRE(scroll->physics()->isRunning() == false);

    delete smi;
}

TEST_CASE("An overscrolled gesture with no end springs back on idle",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);
    REQUIRE(scroll->snap() == false);

    rive::Vec2D over(50.0f, 250.0f);

    // A precise drag writes the offset unclamped, so pushing past the end
    // leaves it out of range with only the elastic clamp hiding the excess.
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::begin, 0, 0));
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -900.0f));
    REQUIRE(scroll->offsetY() < scroll->maxOffsetY());
    REQUIRE(scroll->isOverscrolled() == true);

    // No end is coming -- this is the macOS / editor path -- so the idle
    // timer has to start the spring back even though the view does not snap.
    // Stepped in frames: one long advance would start AND finish the settle,
    // since the decay clamps at elapsed * friction >= 1.
    bool settling = false;
    for (int i = 0; i < 20 && !settling; i++)
    {
        smi->advanceAndApply(0.016f);
        settling = scroll->physics()->isRunning();
    }
    REQUIRE(settling == true);

    // Let it settle.
    for (int i = 0; i < 300 && scroll->physics()->isRunning(); i++)
    {
        smi->advanceAndApply(0.016f);
    }
    REQUIRE(scroll->offsetY() == Approx(scroll->maxOffsetY()));
    REQUIRE(scroll->isOverscrolled() == false);

    delete smi;
}

TEST_CASE("A precise gesture stretches an elastic view at its edge",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    rive::Vec2D over(50.0f, 250.0f);

    // Sitting at the top, so there is no movement available in this direction.
    REQUIRE(scroll->offsetY() == 0.0f);
    REQUIRE(scroll->canConsume(rive::Vec2D(0, 40.0f)) == false);

    // A detented wheel declines and chains outward...
    REQUIRE(smi->pointerScroll(over, wheel(0, 40.0f)) == rive::HitResult::none);
    REQUIRE(scroll->offsetY() == 0.0f);

    // ...but a precise gesture pulls the rubber band instead of passing the
    // gesture on, which is what the platforms do.
    REQUIRE(scroll->canStretch(rive::Vec2D(0, 40.0f)) == true);
    REQUIRE(smi->pointerScroll(over,
                               trackpad(rive::ScrollPhase::update, 0, 40.0f)) !=
            rive::HitResult::none);
    REQUIRE(scroll->offsetY() > 0.0f);
    REQUIRE(scroll->isOverscrolled() == true);

    // The visible give is damped well below the raw pull.
    REQUIRE(scroll->clampedOffsetY() < scroll->offsetY());

    delete smi;
}

TEST_CASE("A settle mid-gesture does not leave the view rendering pinned",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    rive::Vec2D over(50.0f, 250.0f);

    // Overscroll, then let inertia bounce it back.
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::begin, 0, 0));
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -900.0f));
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::momentum, 0, -20.0f));
    REQUIRE(scroll->physics()->isRunning() == true);

    // Run the settle out, with the coast still feeding momentum as a real
    // one does -- those events are what hold the gesture open, since the idle
    // timer is reset by events and not by frames. Finishing the settle calls
    // reset(), which destroys the elastic helpers mid-gesture.
    for (int i = 0; i < 300 && scroll->physics()->isRunning(); i++)
    {
        smi->advanceAndApply(0.016f);
        smi->pointerScroll(over,
                           trackpad(rive::ScrollPhase::momentum, 0, -1.0f));
    }
    REQUIRE(scroll->isScrolling() == true);
    REQUIRE(scroll->physics()->enabled() == false);

    // A new pull in the same gesture has to re-prime, or the offset
    // accumulates while the rendered position stays hard-clamped at the edge.
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -60.0f));
    REQUIRE(scroll->physics()->enabled() == true);
    REQUIRE(scroll->clampedOffsetY() < scroll->maxOffsetY());

    delete smi;
}

TEST_CASE("Interest follows dragMultiplier, not the raw delta", "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    rive::Vec2D over(50.0f, 250.0f);

    // dragMultiplier is bindable and unclamped. At zero the view cannot move
    // at all, so it must decline rather than latch and swallow the gesture.
    scroll->dragMultiplier(0.0f);
    REQUIRE(scroll->canConsume(rive::Vec2D(0, -50.0f)) == false);
    REQUIRE(scroll->canStretch(rive::Vec2D(0, -50.0f)) == false);
    REQUIRE(smi->pointerScroll(over, wheel(0, -50.0f)) ==
            rive::HitResult::none);

    // A negative multiplier inverts the gesture, so interest has to be judged
    // at the edge the content will actually travel toward.
    scroll->dragMultiplier(-1.0f);
    REQUIRE(scroll->offsetY() == 0.0f);
    REQUIRE(scroll->canConsume(rive::Vec2D(0, 50.0f)) == true);
    REQUIRE(scroll->canConsume(rive::Vec2D(0, -50.0f)) == false);

    smi->pointerScroll(over, wheel(0, 50.0f));
    REQUIRE(scroll->offsetY() == -50.0f);

    delete smi;
}

TEST_CASE("A no-begin gesture does not derive velocity from its first delta",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    rive::Vec2D over(50.0f, 250.0f);

    // Wayland and the editor send no begin, so this delta both primes physics
    // and carries movement. prepare() seeds the velocity clock to now, so
    // measuring this delta against it would divide by microseconds -- the
    // speed, and the acceleration derived from it, would be astronomical and
    // the eventual fling wild.
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -40.0f));
    REQUIRE(scroll->offsetY() == -40.0f);
    REQUIRE(scroll->velocityY() == 0.0f);

    // The next delta has a real interval behind it, so it establishes speed.
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -40.0f));
    REQUIRE(scroll->offsetY() == -80.0f);
    REQUIRE(scroll->velocityY() != 0.0f);

    delete smi;
}

TEST_CASE("A no-begin drag takes control from a running fling", "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    rive::Vec2D over(50.0f, 250.0f);

    // Throw it, so a fling is running.
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::begin, 0, 0));
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -40.0f));
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -40.0f));
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::end, 0, 0));
    REQUIRE(scroll->physics()->isRunning() == true);
    smi->advanceAndApply(0.016f);

    // A host with no begin opens its next gesture with a plain update. It has
    // to take the view over: otherwise the simulation keeps running and the
    // next advance overwrites whatever the drag just did.
    float before = scroll->offsetY();
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -25.0f));
    REQUIRE(scroll->physics()->isRunning() == false);
    REQUIRE(scroll->offsetY() == Approx(before - 25.0f));

    // The drag survives the frame rather than being replaced by the fling.
    float dragged = scroll->offsetY();
    smi->advanceAndApply(0.016f);
    REQUIRE(scroll->offsetY() == Approx(dragged));

    delete smi;
}

TEST_CASE("A zero-delta begin does not pick the target for the gesture",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/scroll_nested.riv");
    NestedScene scene;
    openNested(file.get(), scene);

    // Flutter sends begin before any delta. It says nothing about the axis,
    // so latching on it would pin the gesture to the strip (topmost here) and
    // swallow every vertical update instead of chaining to the list.
    REQUIRE(
        scene.smi->pointerScroll(scene.overStrip,
                                 trackpad(rive::ScrollPhase::begin, 0, 0)) ==
        rive::HitResult::none);
    REQUIRE(scene.smi->hasScrollLatch() == false);

    // The first directional update chooses, and chooses correctly.
    scene.smi->pointerScroll(scene.overStrip,
                             trackpad(rive::ScrollPhase::update, 0, -50.0f));
    REQUIRE(scene.outer->offsetY() == -50.0f);
    REQUIRE(scene.strip->offsetX() == 0.0f);

    delete scene.smi;
}

TEST_CASE("A clamped view keeps accumulating trackpad velocity",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    // The asset's physics is elastic. Clamped physics has no helpers for a
    // settle to destroy, so a gesture must never read it as unprimed and
    // re-prime on every delta: that would zero the velocity each time.
    delete scroll->physics();
    scroll->physics(new rive::ClampedScrollPhysics());
    smi->advanceAndApply(0.0f);

    rive::Vec2D over(50.0f, 250.0f);
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -40.0f));
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -40.0f));
    smi->pointerScroll(over, trackpad(rive::ScrollPhase::update, 0, -40.0f));
    REQUIRE(scroll->offsetY() == -120.0f);
    // Only the opening delta primes the clock; every later one sets the speed.
    REQUIRE(scroll->velocityY() != 0.0f);

    delete smi;
}

TEST_CASE("A wheel gesture going idle does not end a pointer drag",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    rive::Vec2D over(50.0f, 250.0f);
    smi->pointerScroll(over, wheel(0, -20.0f));
    REQUIRE(scroll->isScrolling() == true);

    // Press inside the idle window, so a pointer drag overlaps the gesture.
    smi->pointerMove(over);
    smi->pointerDown(over);
    REQUIRE(scroll->isDragging() == true);

    // The wheel gesture closes; the drag it overlapped is still down.
    advancePastIdle(smi);
    REQUIRE(scroll->isScrolling() == false);
    REQUIRE(scroll->isDragging() == true);

    smi->pointerUp(over);
    REQUIRE(scroll->isDragging() == false);

    delete smi;
}

TEST_CASE("Wheel input can be switched off per view while drag still works",
          "[scrollinput]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("State Machine 1");
    rive::StateMachineInstance* smi =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());
    auto scroll = artboardInstance->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);

    // wheelInteractive is bit 0 of scrollFlags and defaults on.
    REQUIRE(scroll->wheelInteractive() == true);
    scroll->wheelInteractive(false);
    REQUIRE(scroll->interactive() == true);

    rive::Vec2D over(50.0f, 250.0f);
    // Neither a target for the host to claim, nor a taker for the event.
    REQUIRE(smi->hasScrollTargetAt(over) == false);
    REQUIRE(smi->pointerScroll(over, wheel(0, -50.0f)) ==
            rive::HitResult::none);
    REQUIRE(scroll->offsetY() == 0.0f);

    // Pointer drag is gated by interactive alone.
    smi->pointerMove(over);
    smi->pointerDown(over);
    smi->pointerMove(rive::Vec2D(50.0f, 150.0f));
    REQUIRE(scroll->offsetY() < 0.0f);
    smi->pointerUp(rive::Vec2D(50.0f, 150.0f));

    delete smi;
}
