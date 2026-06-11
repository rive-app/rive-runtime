#include <rive/file.hpp>
#include <rive/node.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

using namespace rive;

TEST_CASE("Computed root values", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/computed_values_test.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    stateMachine->bindViewModelInstance(vmi);

    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    int frames = (int)(2.0f / 0.032f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.032f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("computed_values_test"));
}
