#include "rive/file.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>

using namespace rive;

TEST_CASE("data bind lists reset triggers", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/viewmodel_list_trigger.riv", &silver);

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

    auto list = vmi->propertyValue("lis")->as<ViewModelInstanceList>();
    auto listItem = list->item(0);
    auto instance = listItem->viewModelInstance();
    auto trigger =
        instance->propertyValue("tri")->as<ViewModelInstanceTrigger>();
    trigger->trigger();

    for (int i = 0; i < 4; i++)
    {
        silver.addFrame();
        trigger->trigger();
        stateMachine->advanceAndApply(0.064f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("viewmodel_list_trigger"));
}
