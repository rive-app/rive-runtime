#include <rive/file.hpp>
#include <rive/node.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "rive/nested_artboard.hpp"
#include "utils/serializing_factory.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive_file_reader.hpp"
#include <cstdio>
#include <catch.hpp>

using namespace rive;

TEST_CASE("Reusing a path in multiple passes works correctly", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/reuse_path_in_effect.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    CHECK(silver.matches("reuse_path_in_effect"));
}

TEST_CASE("A clip follows the path effect on its source's fill", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/scripted_path_effect_clip.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    // Multiple frames: a single one passes even if the clip resolves the
    // effect once and then freezes, which is how this broke in the editor.
    for (int i = 0; i < 60; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(1.0f / 60.0f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("scripted_path_effect_clip"));
}
