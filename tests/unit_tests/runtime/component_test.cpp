#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/text/text.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("Stateful Component ViewModelInstance", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/component_stateful_vm_instance.riv", &silver);

    auto artboard = file->artboardNamed("ParentArtboard");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto heightProperty = vmi->propertyValue("h");
    REQUIRE(heightProperty != nullptr);
    REQUIRE(heightProperty->is<rive::ViewModelInstanceNumber>());
    heightProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(200);

    int frames = 30;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    heightProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(50);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("component_stateful_vm_instance"));
}

TEST_CASE("Stateful Component ViewModelInstance multi", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/component_stateful_vm_instance_2.riv", &silver);

    auto artboard = file->artboardNamed("Artboard");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto labelProperty = vmi->propertyValue("label");
    REQUIRE(labelProperty != nullptr);
    REQUIRE(labelProperty->is<rive::ViewModelInstanceString>());

    int frames = 30;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    labelProperty->as<rive::ViewModelInstanceString>()->propertyValue(
        "Override");
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("component_stateful_vm_instance_2"));
}