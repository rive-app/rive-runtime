#include "rive/file.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "utils/serializing_factory.hpp"
#include "../include/pointer_log_replay.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace rive;

// These rivs carry Luau bytecode; without the Luau backend the scripts
// cannot run and the streams cannot match.
#ifdef WITH_RIVE_SCRIPTING_LUAU

TEST_CASE("Game menu ad script test", "[script]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/game_menu_ad_police_files.riv", &silver);
    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());
    auto sm = artboard->stateMachineAt(0);
    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);
    sm->bindViewModelInstance(vmi);
    sm->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    replayPointerLog("assets/game_menu_ad_police_files.json",
                     silver,
                     artboard.get(),
                     sm.get(),
                     renderer.get(),
                     {.fps = 30});
    CHECK(silver.matches("game_menu_ad_police_files"));
}

TEST_CASE("Inventory script test", "[script]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/inventory_demo_test_v2.riv", &silver);
    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());
    auto sm = artboard->stateMachineAt(0);
    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);
    sm->bindViewModelInstance(vmi);
    sm->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    replayPointerLog("assets/inventory_demo_test_v2.json",
                     silver,
                     artboard.get(),
                     sm.get(),
                     renderer.get(),
                     {.fps = 30});
    CHECK(silver.matches("inventory_demo_test_v2"));
}
TEST_CASE("Layout Planets script test", "[script]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/layoutstest_8-planets-grid.riv", &silver);
    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());
    auto sm = artboard->stateMachineAt(0);
    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);
    sm->bindViewModelInstance(vmi);
    sm->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    replayPointerLog("assets/layoutstest_8-planets-grid.json",
                     silver,
                     artboard.get(),
                     sm.get(),
                     renderer.get(),
                     {.fps = 30});
    CHECK(silver.matches("layoutstest_8-planets-grid"));
}

#endif // WITH_RIVE_SCRIPTING_LUAU