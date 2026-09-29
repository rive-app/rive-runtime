#include "rive/animation/blend_accumulator.hpp"
#include "rive/animation/blend_state_1d.hpp"
#include "rive/animation/layer_state_flags.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/node.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

// Mixing through the accumulator has to land exactly where writing each value
// to the object would have.
TEST_CASE("a blend accumulator mixes like writes to the object",
          "[statemachine]")
{
    auto file = ReadRiveFile("assets/blend_test.riv");
    auto artboard = file->artboardDefault();
    auto nodes = artboard->find<rive::Node>();
    REQUIRE(nodes.size() >= 2);
    auto* seeded = nodes[0];
    auto* unseeded = nodes[1];
    const int x = rive::NodeBase::xPropertyKey;
    unseeded->x(7.0f);

    rive::BlendAccumulator accumulator;
    accumulator.seedDouble(seeded, x, 10.0f);
    accumulator.applyDouble(seeded, x, 0.5f, 30.0f);
    accumulator.applyDouble(seeded, x, 0.25f, 50.0f);
    // Without a seed, the first write mixes with the object's value.
    accumulator.applyDouble(unseeded, x, 0.5f, 20.0f);
    // Nothing reaches the objects until the flush.
    CHECK(unseeded->x() == 7.0f);
    accumulator.flush();

    float expected = 10.0f;
    expected = expected * 0.5f + 30.0f * 0.5f;
    expected = expected * 0.75f + 50.0f * 0.25f;
    CHECK(seeded->x() == expected);
    CHECK(unseeded->x() == 7.0f * 0.5f + 20.0f * 0.5f);

    // The next frame starts over from the objects' values.
    accumulator.applyDouble(unseeded, x, 0.5f, 0.0f);
    accumulator.flush();
    CHECK(unseeded->x() == (7.0f * 0.5f + 20.0f * 0.5f) * 0.5f);
}

// A blend with a reset wrote every property it blends through its reset value
// each frame before mixing the animations back in, marking it changed and
// dirtying everything downstream even when the blend came out where it was.
TEST_CASE("a settled blend state doesn't dirty what it blends",
          "[statemachine]")
{
    auto file = ReadRiveFile("assets/blend_test.riv");
    // Give the file's blend states the reset newer files turn on (this one
    // predates it), before any instance is made from them.
    auto definition = file->artboard()->stateMachine("blend");
    REQUIRE(definition != nullptr);
    auto layer = definition->layer(0);
    int blendStates = 0;
    for (size_t i = 0; i < layer->stateCount(); i++)
    {
        auto* state = layer->state(i);
        if (state->is<rive::BlendState1D>())
        {
            state->flags(state->flags() |
                         (uint32_t)rive::LayerStateFlags::Reset);
            blendStates++;
        }
    }
    REQUIRE(blendStates > 0);

    auto artboard = file->artboardDefault();
    auto machine = artboard->stateMachineNamed("blend");
    REQUIRE(machine != nullptr);
    for (int i = 0; i < 60; i++)
    {
        machine->advanceAndApply(1.0f / 60.0f);
    }
    REQUIRE_FALSE(artboard->hasDirt(rive::ComponentDirt::Components));

    // Applying again without moving time comes out the same, so nothing may
    // be marked dirty.
    machine->advance(0.0f);
    CHECK_FALSE(artboard->hasDirt(rive::ComponentDirt::Components));
}
