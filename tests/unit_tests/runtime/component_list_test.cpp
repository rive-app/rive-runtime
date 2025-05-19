#include <rive/animation/state_machine_input_instance.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/text/text.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"
#include "utils/no_op_factory.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("Component List Artboard Count", "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_1.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");

    artboard->advance(0.0f);

    REQUIRE(list->syncStyleChanges() == true);
    REQUIRE(list->artboardCount() == 8);
    REQUIRE(list->layoutNode(9) == nullptr);
    REQUIRE(list->artboardInstance(9) == nullptr);
    REQUIRE(list->stateMachineInstance(9) == nullptr);
}

TEST_CASE("Component List Artboards & State Machines", "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_1.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");

    artboard->advance(0.0f);

    for (int i = 0; i < list->artboardCount(); i++)
    {
        auto artboard = list->artboardInstance(i);
        REQUIRE(artboard != nullptr);
        REQUIRE(artboard->name() == "Item");
        auto sm = list->stateMachineInstance(i);
        REQUIRE(sm != nullptr);
        REQUIRE(sm->artboard() == artboard);
    }
}

TEST_CASE("Component List Artboards Layout Nodes", "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_1.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");

    artboard->advance(0.0f);

    for (int i = 0; i < list->artboardCount(); i++)
    {
        auto node = list->layoutNode(i);
        REQUIRE(node != nullptr);
    }
}

TEST_CASE("Component List Artboards Layout Bounds", "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_1.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");

    artboard->advance(0.0f);

    for (int i = 0; i < list->artboardCount(); i++)
    {
        auto artboard = list->artboardInstance(i);
        REQUIRE(artboard != nullptr);
        auto bounds = artboard->layoutBounds();
        // Artboards are using Column layout
        REQUIRE(bounds.top() == i * 60);
    }
}

TEST_CASE("Component List Artboards Data Context", "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_1.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");

    artboard->advance(0.0f);

    std::vector<std::string> labels =
        {"ONE", "TWO", "THREE", "THREE", "THREE", "THREE", "TWO", "ONE"};
    for (int i = 0; i < list->artboardCount(); i++)
    {
        auto artboard = list->artboardInstance(i);
        REQUIRE(artboard != nullptr);
        auto context = artboard->dataContext();
        auto vmString = context->viewModelInstance()->propertyValue("Label");
        REQUIRE(vmString->is<rive::ViewModelInstanceString>());
        REQUIRE(
            vmString->as<rive::ViewModelInstanceString>()->propertyValue() ==
            labels[i]);
    }
}

TEST_CASE("Component List Artboards Labels", "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_1.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");

    artboard->advance(0.0f);

    std::vector<std::string> labels =
        {"ONE", "TWO", "THREE", "THREE", "THREE", "THREE", "TWO", "ONE"};
    for (int i = 0; i < list->artboardCount(); i++)
    {
        auto artboard = list->artboardInstance(i);
        REQUIRE(artboard != nullptr);
        auto label = artboard->find("TextLabel");
        REQUIRE(label->is<rive::Text>());
        REQUIRE(label->as<rive::Text>()->runs()[0]->text() == labels[i]);
    }
}

TEST_CASE("Component list state machine listener", "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_1.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachine("State Machine 1");
    REQUIRE(stateMachine != nullptr);
    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, artboard.get());
    REQUIRE(stateMachineInstance != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");

    artboard->advance(0.0f);

    auto stateMachineInstance1 = list->stateMachineInstance(0);
    auto hoverInput1 = stateMachineInstance1->getBool("Hover");
    REQUIRE(hoverInput1 != nullptr);
    REQUIRE(hoverInput1->value() == false);

    REQUIRE(stateMachineInstance1->hitComponentsCount() == 1);
    // Move over the first shape
    stateMachineInstance->pointerMove(rive::Vec2D(100.0f, 30.0f));
    artboard->advance(0.0f);
    stateMachineInstance->advanceAndApply(0.0f);
    {
        REQUIRE(hoverInput1->value() == true);
    }

    auto stateMachineInstance3 = list->stateMachineInstance(2);
    auto hoverInput3 = stateMachineInstance3->getBool("Hover");
    REQUIRE(hoverInput3 != nullptr);
    REQUIRE(hoverInput3->value() == false);

    REQUIRE(stateMachineInstance3->hitComponentsCount() == 1);
    // Move over the first shape
    stateMachineInstance->pointerMove(rive::Vec2D(100.0f, 150.0f));
    artboard->advance(0.0f);
    stateMachineInstance->advanceAndApply(0.0f);
    {
        REQUIRE(hoverInput3->value() == true);
    }
    delete stateMachineInstance;
}

TEST_CASE("Number To List Artboard Count", "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_2.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");

    artboard->advance(0.0f);

    REQUIRE(list->syncStyleChanges() == true);
    REQUIRE(list->artboardCount() == 12);
    REQUIRE(list->layoutNode(13) == nullptr);
    REQUIRE(list->artboardInstance(13) == nullptr);
    REQUIRE(list->stateMachineInstance(13) == nullptr);
    for (int i = 0; i < list->artboardCount(); i++)
    {
        REQUIRE(list->listItem(i) != nullptr);
    }
    auto countProperty = viewModelInstance->propertyValue("ItemCount");
    countProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(6);

    artboard->advance(0.0f);
    REQUIRE(list->artboardCount() == 6);
}

TEST_CASE("Number To List Artboards & State Machines", "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_2.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");

    artboard->advance(0.0f);

    for (int i = 0; i < list->artboardCount(); i++)
    {
        auto artboard = list->artboardInstance(i);
        REQUIRE(artboard != nullptr);
        REQUIRE(artboard->name() == "Item");
        auto sm = list->stateMachineInstance(i);
        REQUIRE(sm != nullptr);
        REQUIRE(sm->artboard() == artboard);
    }
}

TEST_CASE("Number To List Artboards Layout Nodes", "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_2.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");

    artboard->advance(0.0f);

    for (int i = 0; i < list->artboardCount(); i++)
    {
        auto node = list->layoutNode(i);
        REQUIRE(node != nullptr);
    }
}

TEST_CASE("Number To List Artboards Data Context", "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_2.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");

    artboard->advance(0.0f);

    for (int i = 0; i < list->artboardCount(); i++)
    {
        auto artboard = list->artboardInstance(i);
        REQUIRE(artboard != nullptr);
        auto item = list->listItem(i);
        auto vmInstance = item->viewModelInstance();
        auto symbol = vmInstance->symbol(
            rive::ViewModelInstanceSymbolListIndexBase::typeKey);
        REQUIRE(symbol != nullptr);
        auto index = symbol->as<rive::ViewModelInstanceSymbolListIndex>()
                         ->propertyValue();
        REQUIRE(index == i);
    }
}

TEST_CASE("Number to List Artboards Labels", "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_2.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");

    artboard->advance(0.0f);

    std::vector<std::string> labels =
        {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"};
    for (int i = 0; i < list->artboardCount(); i++)
    {
        auto artboard = list->artboardInstance(i);
        REQUIRE(artboard != nullptr);
        auto label = artboard->find("ItemLabel");
        REQUIRE(label->is<rive::Text>());
        REQUIRE(label->as<rive::Text>()->runs()[0]->text() == labels[i]);
    }
}