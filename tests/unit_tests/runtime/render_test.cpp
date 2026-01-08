#include "rive/file.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_renderer.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>

using namespace rive;

TEST_CASE("file with only solid color animating triggers change on artboard",
          "[silver]")
{
    auto file = ReadRiveFile("assets/solid_affects_has_changed.riv");

    auto artboard = file->artboardDefault();
    NoOpRenderer renderer;

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(&renderer);

    int frames = 10;
    for (int i = 0; i < frames; i++)
    {
        stateMachine->advanceAndApply(0.1f);
        REQUIRE(artboard->didChange() == true);
        artboard->draw(&renderer);
    }
}