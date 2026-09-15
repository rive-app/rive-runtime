#include "rive/animation/state_machine_instance.hpp"
#include "rive/file.hpp"
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"
#include <catch.hpp>

using namespace rive;

// Rendering coverage for grid/stack layouts. Each artboard in the asset drives
// its state machine so animated track sizing, spans and participants are
// exercised over time, not just at rest.
static const int kFrames = 120;

static void gridStackSilver(const char* artboardName, const char* silverName)
{
    INFO("artboard: " << artboardName);
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/layout_grid_stack.riv", &silver);

    auto artboard = file->artboardNamed(artboardName);
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    // Settle the initial state without consuming time, then draw frame 0.
    stateMachine->advanceAndApply(0.0f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    for (int i = 0; i < kFrames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches(silverName));
}

TEST_CASE("grid with layouts silver", "[silver]")
{
    gridStackSilver("GridWithLayouts", "layout_grid_stack_grid_with_layouts");
}

TEST_CASE("stack with layouts silver", "[silver]")
{
    gridStackSilver("StackWithLayouts", "layout_grid_stack_stack_with_layouts");
}

TEST_CASE("grid with layouts size changing silver", "[silver]")
{
    gridStackSilver("GridWithLayoutsSizeChanging",
                    "layout_grid_stack_grid_with_layouts_size_changing");
}

TEST_CASE("grid with layouts span silver", "[silver]")
{
    gridStackSilver("GridWithLayoutsSpan",
                    "layout_grid_stack_grid_with_layouts_span");
}

TEST_CASE("grid with layouts size span changing silver", "[silver]")
{
    gridStackSilver("GridWithLayoutsSizeSpanChanging",
                    "layout_grid_stack_grid_with_layouts_size_span_changing");
}

TEST_CASE("grid with layout participants silver", "[silver]")
{
    gridStackSilver("GridWithLayoutParticipants",
                    "layout_grid_stack_grid_with_layout_participants");
}
