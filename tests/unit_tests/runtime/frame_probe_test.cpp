/*
 * Copyright 2026 Rive
 */

// Probes for work a settled scene should not be doing. Each settles a file,
// then counts over a window of frames how many layout passes ran and how many
// frames still asked to keep going.
//
// The report-only sweep is hidden behind a dot tag; run it from
// tests/unit_tests with:
//
//     ./out/debug/unit_tests "[.frame_probe]"
//
// It prints instead of asserting, to record today's behaviour. Once a fix
// removes a keep-alive, the files it covers get an asserting test below.

#include "rive/artboard.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/file.hpp"
#include "rive/layout_component.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout/layout_enums.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <cstdio>

using namespace rive;

namespace
{
constexpr float kFrameSeconds = 1.0f / 60.0f;
constexpr int kSettleFrames = 120;
constexpr int kProbeFrames = 60;

struct ProbeResult
{
    uint64_t layoutPasses = 0;
    int keepGoingFrames = 0;
};

void report(const char* kind, const char* path, const ProbeResult& result)
{
    printf("[frame_probe] %-8s %-52s layout passes %3llu / %d frames, "
           "keep-going %2d / %d\n",
           kind,
           path,
           (unsigned long long)result.layoutPasses,
           kProbeFrames,
           result.keepGoingFrames,
           kProbeFrames);
}

// The artboard on its own: no state machine, so nothing authored animates.
// Anything still updating once settled is the runtime keeping itself busy.
ProbeResult probeArtboard(const char* path)
{
    auto file = ReadRiveFile(path);
    auto artboard = file->artboardDefault();
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    if (viewModelInstance != nullptr)
    {
        artboard->bindViewModelInstance(viewModelInstance);
    }
    for (int i = 0; i < kSettleFrames; ++i)
    {
        artboard->advance(kFrameSeconds);
    }
    ProbeResult result;
    uint64_t before = Artboard::layoutPassCount();
    for (int i = 0; i < kProbeFrames; ++i)
    {
        result.keepGoingFrames += artboard->advance(kFrameSeconds) ? 1 : 0;
    }
    result.layoutPasses = Artboard::layoutPassCount() - before;
    return result;
}

// The artboard driven by its default state machine, as a host drives it. A
// positive time step: advanceAndApply(0) always reports keep-going.
ProbeResult probeScene(const char* path)
{
    auto file = ReadRiveFile(path);
    auto artboard = file->artboardDefault();
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    if (viewModelInstance != nullptr)
    {
        artboard->bindViewModelInstance(viewModelInstance);
    }
    std::unique_ptr<StateMachineInstance> stateMachine =
        artboard->defaultStateMachine();
    if (stateMachine == nullptr && artboard->stateMachineCount() > 0)
    {
        stateMachine = artboard->stateMachineAt(0);
    }
    if (stateMachine == nullptr)
    {
        return probeArtboard(path);
    }
    if (viewModelInstance != nullptr)
    {
        stateMachine->bindViewModelInstance(viewModelInstance);
    }
    for (int i = 0; i < kSettleFrames; ++i)
    {
        stateMachine->advanceAndApply(kFrameSeconds);
    }
    ProbeResult result;
    uint64_t before = Artboard::layoutPassCount();
    for (int i = 0; i < kProbeFrames; ++i)
    {
        result.keepGoingFrames +=
            stateMachine->advanceAndApply(kFrameSeconds) ? 1 : 0;
    }
    result.layoutPasses = Artboard::layoutPassCount() - before;
    return result;
}

// The first shape in [artboard] that takes part in layout through a
// LayoutParticipant and has a path to move.
Shape* firstParticipantShape(Artboard* artboard)
{
    for (auto shape : artboard->find<Shape>())
    {
        if (shape->isParticipatingInLayout() && !shape->paths().empty())
        {
            return shape;
        }
    }
    return nullptr;
}

const char* kFiles[] = {
    // Layout participants.
    "assets/layout/stack_participant.riv",
    "assets/layout/hug_participant.riv",
    "assets/layout/scroll_participant.riv",
    "assets/layout/group_participant.riv",
    "assets/layout/solo_participant.riv",
    "assets/layout/animated_participant.riv",
    "assets/layout_grid_stack.riv",
    // Layouts without participants, as controls.
    "assets/layout/layout_paint.riv",
    "assets/layout/layout_display.riv",
    // Text in layouts.
    "assets/layout_text_match.riv",
    // Interpolating converters (an uninitialized one reports keep-going).
    "assets/data_converter_interpolator_reset.riv",
    "assets/interpolation_zero_duration.riv",
    "assets/time_based_interpolation.riv",
    "assets/interpolate_to_end.riv",
    // Larger real files.
    "assets/db_health_tracker.riv",
    "assets/data_viz_demo.riv",
    "assets/car_widgets_v01.riv",
    "assets/superbowl.riv",
};
} // namespace

TEST_CASE("frame probe: settled artboards without a state machine",
          "[.frame_probe]")
{
    for (auto path : kFiles)
    {
        report("artboard", path, probeArtboard(path));
    }
}

TEST_CASE("frame probe: settled scenes with their state machine",
          "[.frame_probe]")
{
    for (auto path : kFiles)
    {
        report("scene", path, probeScene(path));
    }
}

// A visible participant used to keep its artboard busy forever: each solve
// dirtied the host's world transform, the shape read that as a change to its
// bounds and asked for another solve. Settled, a scene of participants must
// stop solving and report that it's done, with or without its state machine.
TEST_CASE("settled layout participants stop re-solving the layout",
          "[layoutparticipant]")
{
    const char* files[] = {
        "assets/layout/stack_participant.riv",
        "assets/layout/hug_participant.riv",
        "assets/layout/scroll_participant.riv",
        "assets/layout/group_participant.riv",
        "assets/layout/solo_participant.riv",
        "assets/layout/animated_participant.riv",
    };
    for (auto path : files)
    {
        INFO(path);
        auto artboard = probeArtboard(path);
        CHECK(artboard.layoutPasses == 0);
        CHECK(artboard.keepGoingFrames == 0);
        auto scene = probeScene(path);
        CHECK(scene.layoutPasses == 0);
        CHECK(scene.keepGoingFrames == 0);
    }
}

// The other half of that contract: a participant must still hear about a
// change in shape space when its world transform can't be inverted -- a
// layout folding it to zero -- so the check can't go through that transform.
TEST_CASE("a folded participant still re-measures when its path moves",
          "[layoutparticipant]")
{
    auto file = ReadRiveFile("assets/layout/stack_participant.riv");
    auto artboard = file->artboardDefault();
    Shape* shape = firstParticipantShape(artboard.get());
    REQUIRE(shape != nullptr);

    // Fold the shape flat along x: its world transform is now singular.
    shape->scaleX(0.0f);
    for (int i = 0; i < kSettleFrames; ++i)
    {
        artboard->advance(kFrameSeconds);
    }
    uint64_t settled = Artboard::layoutPassCount();
    artboard->advance(kFrameSeconds);
    REQUIRE(Artboard::layoutPassCount() == settled);

    // Move a path along the folded axis: nothing moves in world space, but
    // the participant's shape-space bounds do.
    auto path = shape->paths()[0];
    path->x(path->x() + 10.0f);
    artboard->advance(kFrameSeconds);
    artboard->advance(kFrameSeconds);
    CHECK(Artboard::layoutPassCount() > settled);
}

// A path can rebuild without changing anything the layout measures -- a
// deformed path rebuilds whenever its world transform changes, and a solve
// always dirties the host's -- and that must not cost another solve.
TEST_CASE("a participant whose path rebuilds in place does not re-solve",
          "[layoutparticipant]")
{
    auto file = ReadRiveFile("assets/layout/stack_participant.riv");
    auto artboard = file->artboardDefault();
    Shape* shape = firstParticipantShape(artboard.get());
    REQUIRE(shape != nullptr);
    for (int i = 0; i < kSettleFrames; ++i)
    {
        artboard->advance(kFrameSeconds);
    }
    uint64_t settled = Artboard::layoutPassCount();
    for (int i = 0; i < kProbeFrames; ++i)
    {
        // Rebuild without messaging the layout ourselves, as a deformer's
        // world-transform rebuild doesn't.
        shape->paths()[0]->markPathDirty(false);
        artboard->advance(kFrameSeconds);
    }
    CHECK(Artboard::layoutPassCount() == settled);
}

// A layout tween moves the layout toward a target its last solve already
// produced, and nothing Yoga reads changes while it runs, so it needs no more
// solves until it ends.
TEST_CASE("a layout tween runs without re-solving the layout", "[layout]")
{
    // A container with a custom layout animation holding six 100x100 layouts
    // that inherit it. The file binds the container's interpolation time,
    // which reads 0 while unbound, so give it a one second linear tween.
    auto file = ReadRiveFile("assets/layout/layout_anim_bound.riv");
    auto artboard = file->artboardDefault();
    LayoutComponent* container = nullptr;
    LayoutComponent* layout = nullptr;
    for (auto* candidate : artboard->find<LayoutComponent>())
    {
        if (candidate->is<Artboard>() || candidate->style() == nullptr)
        {
            continue;
        }
        auto style = candidate->animationStyle();
        if (container == nullptr && style == LayoutAnimationStyle::custom)
        {
            container = candidate;
        }
        if (layout == nullptr && style == LayoutAnimationStyle::inherit)
        {
            layout = candidate;
        }
    }
    REQUIRE(container != nullptr);
    REQUIRE(layout != nullptr);
    container->style()->interpolationType(
        (uint8_t)LayoutStyleInterpolation::linear);
    container->style()->interpolationTime(1.0f);
    artboard->advance(0.0f);
    REQUIRE(layout->layoutWidth() == 100.0f);

    // Shrinking the layout solves once, which starts the tween.
    layout->width(50.0f);
    artboard->advance(kFrameSeconds);
    uint64_t retargeted = Artboard::layoutPassCount();
    int frames = 0;
    while (layout->layoutWidth() > 50.0f && frames < 2 * kProbeFrames)
    {
        artboard->advance(kFrameSeconds);
        frames++;
    }
    CHECK(layout->layoutWidth() == 50.0f);
    CHECK(frames > 1);
    CHECK(Artboard::layoutPassCount() == retargeted);
}
