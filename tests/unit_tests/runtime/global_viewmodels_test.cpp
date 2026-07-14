#include "rive/file.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"
#include "rive/math/random.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>
#include "rive/profiler/profiler_macros.h"
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace rive;

TEST_CASE("Global view models and overrides", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/global_variables_test.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    // Globals are no longer auto-created at instance time; create + set a
    // default for each (as the high-level runtime's autoBind does), then apply
    // everything with a single bind.
    stateMachine->setViewModelInstance(vmi);
    for (auto& name : file->globalViewModelNames())
    {
        auto global =
            file->createDefaultViewModelInstance(file->viewModel(name));
        REQUIRE(global != nullptr);
        REQUIRE(stateMachine->setGlobalViewModelInstance(name, global));
    }
    stateMachine->bind();
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    CHECK(silver.matches("global_variables_test"));
}

TEST_CASE("Test global view models with automatic instancing", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/global_viewmodels_test.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    auto renderer = silver.makeRenderer();
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("global_viewmodels_test-auto_instance"));
}

TEST_CASE("Test global view models with instance explicitly specified",
          "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/global_viewmodels_test.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto renderer = silver.makeRenderer();

    {
        auto vmi = file->createDefaultViewModelInstance(artboard.get());
        auto globalColorsVM = file->viewModel("GlobalColors");
        auto vmiColors = file->createDefaultViewModelInstance(globalColorsVM);
        auto c1Prop =
            vmiColors->propertyValue("c1")->as<ViewModelInstanceColor>();

        auto yellowColor = (255 << 24) | (255 << 16) | (255 << 8);
        c1Prop->propertyValue(yellowColor);

        stateMachine->setViewModelInstance(vmi);
        stateMachine->setGlobalViewModelInstance("GlobalColors", vmiColors);
        stateMachine->bind();
    }

    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    {
        auto vmi = file->createDefaultViewModelInstance(artboard.get());
        auto labelProp =
            vmi->propertyValue("label")->as<ViewModelInstanceString>();
        labelProp->propertyValue("label updated");
        auto globalColorsVM = file->viewModel("GlobalColors");
        auto vmiColors = file->createDefaultViewModelInstance(globalColorsVM);
        auto c1Prop =
            vmiColors->propertyValue("c1")->as<ViewModelInstanceColor>();

        auto cyanColor = (255 << 24) | (255 << 8) | 255;
        c1Prop->propertyValue(cyanColor);

        stateMachine->setGlobalViewModelInstance("GlobalColors", vmiColors);
        stateMachine->setViewModelInstance(vmi);
        stateMachine->bind();
    }
    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("global_viewmodels_test-set_instance"));
}