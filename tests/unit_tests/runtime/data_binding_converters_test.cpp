#include "rive/file.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>

using namespace rive;

TEST_CASE("list to length converter", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/list_to_length_test.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto list = vmi->propertyValue("lis");
    auto childVM = file->viewModel("child");
    int cnt = 0;
    while (cnt++ < 4)
    {
        silver.addFrame();
        auto childVMI = file->createDefaultViewModelInstance(childVM);
        if (childVMI != nullptr)
        {
            auto listItem = make_rcp<ViewModelInstanceListItem>();
            listItem->viewModelInstance(childVMI);
            list->as<ViewModelInstanceList>()->addItem(listItem);
        }
        // first advance to set view model value
        stateMachine->advanceAndApply(0.1f);
        // second advance to measure
        stateMachine->advanceAndApply(0.1f);

        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("list_to_length_test"));
}

TEST_CASE("data converter interpolator resets on binding", "[silver]")
{
    SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/data_converter_interpolator_reset.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    auto renderer = silver.makeRenderer();
    int viewModelId = artboard.get()->viewModelId();
    {
        auto vmi = viewModelId == -1
                       ? file->createViewModelInstance(artboard.get())
                       : file->createViewModelInstance(viewModelId, 0);
        auto numProp =
            vmi->propertyValue("xPos")->as<ViewModelInstanceNumber>();
        numProp->propertyValue(250);
        auto colProp = vmi->propertyValue("col")->as<ViewModelInstanceColor>();
        auto redColor = (255 << 24) | (255 << 16);
        colProp->propertyValue(redColor);

        stateMachine->bindViewModelInstance(vmi);
        stateMachine->advanceAndApply(0.1f);

        artboard->draw(renderer.get());

        auto greenColor = (255 << 24) | (255 << 8);
        colProp->propertyValue(greenColor);
        numProp->propertyValue(500);

        int frames = (int)(1.0f / 0.016f);
        for (int i = 0; i < frames; i++)
        {
            silver.addFrame();
            stateMachine->advanceAndApply(0.016f);
            artboard->draw(renderer.get());
        }
    }
    // When a new binding is applied, interpolators are reset and the initial
    // value is not interpolated
    {
        silver.addFrame();
        auto vmi = viewModelId == -1
                       ? file->createViewModelInstance(artboard.get())
                       : file->createViewModelInstance(viewModelId, 0);
        auto numProp =
            vmi->propertyValue("xPos")->as<ViewModelInstanceNumber>();
        numProp->propertyValue(250);
        auto colProp = vmi->propertyValue("col")->as<ViewModelInstanceColor>();
        auto redColor = (255 << 24) | (255 << 16);
        colProp->propertyValue(redColor);
        stateMachine->bindViewModelInstance(vmi);
        stateMachine->advanceAndApply(0.1f);

        artboard->draw(renderer.get());

        auto blueColor = (255 << 24) | 255;
        colProp->propertyValue(blueColor);
        numProp->propertyValue(0);

        int frames = (int)(1.0f / 0.016f);
        for (int i = 0; i < frames; i++)
        {
            silver.addFrame();
            stateMachine->advanceAndApply(0.016f);
            artboard->draw(renderer.get());
        }
    }

    CHECK(silver.matches("data_converter_interpolator_reset"));
}