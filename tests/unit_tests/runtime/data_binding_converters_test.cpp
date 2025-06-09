#include "rive/file.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
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
    std::vector<ViewModelInstanceListItem*> items;
    while (cnt++ < 4)
    {
        silver.addFrame();
        auto childVMI = file->createDefaultViewModelInstance(childVM);
        if (childVMI != nullptr)
        {
            auto listItem = new ViewModelInstanceListItem();
            items.push_back(listItem);
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
    for (auto& item : items)
    {
        delete item;
    }
}