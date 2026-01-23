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

TEST_CASE("Component List Virtualized Artboards & State Machines",
          "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_virtualized.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");
    artboard->advance(0.0f);

    REQUIRE(list->artboardCount() == 20);
    // Only the first 5 items should be created
    for (int i = 0; i < list->artboardCount(); i++)
    {
        auto artboard = list->artboardInstance(i);
        if (i < 5)
        {
            REQUIRE(artboard != nullptr);
            REQUIRE(artboard->name() == "ItemArtboard");
            auto sm = list->stateMachineInstance(i);
            REQUIRE(sm != nullptr);
            REQUIRE(sm->artboard() == artboard);
        }
        else
        {
            REQUIRE(artboard == nullptr);
            auto sm = list->stateMachineInstance(i);
            REQUIRE(sm == nullptr);
        }
    }
}

TEST_CASE("Component List Virtualized Artboards Layout Bounds",
          "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_virtualized.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");
    REQUIRE(list->virtualizationEnabled() == true);

    artboard->advance(0.0f);

    float gap = 10;
    float artboardWidth = 100;
    for (int i = 0; i < list->artboardCount(); i++)
    {
        auto artboard = list->artboardInstance(i);
        if (i < 5)
        {
            REQUIRE(artboard != nullptr);
            auto bounds = list->layoutBoundsForNode(i);
            REQUIRE(bounds.left() == i * (artboardWidth + gap));
        }
        else
        {
            REQUIRE(artboard == nullptr);
        }
    }
}

TEST_CASE("Component List Virtualized Scroll", "[component_list]")
{
    auto file = ReadRiveFile("assets/component_list_virtualized.riv");

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);

    REQUIRE(artboard->find<rive::ScrollConstraint>().size() == 1);
    REQUIRE(artboard->find<rive::ScrollConstraint>()[0] != nullptr);
    auto scroll = artboard->find<rive::ScrollConstraint>()[0];

    REQUIRE(scroll->offsetX() == 0);

    artboard->advance(0.0f);

    // scrollIndex
    scroll->setScrollIndex(2);
    REQUIRE(scroll->infinite() == true);
    REQUIRE(scroll->scrollItemCount() == 20);
    REQUIRE(scroll->offsetX() == -220.0f);
    REQUIRE(scroll->clampedOffsetX() == -220.0f);
    REQUIRE(scroll->minOffsetX() == std::numeric_limits<float>::infinity());
    REQUIRE(scroll->maxOffsetX() == -std::numeric_limits<float>::infinity());
    REQUIRE(scroll->offsetY() == 0.0f);
    REQUIRE(scroll->minOffsetY() == 0.0f);
    REQUIRE(scroll->maxOffsetY() == 0.0f);
    REQUIRE(scroll->clampedOffsetY() == 0.0f);
    REQUIRE(scroll->scrollIndex() == 2);
    REQUIRE(scroll->contentWidth() == 2200.0f);
    REQUIRE(scroll->viewportWidth() == 500.0f);
}

TEST_CASE("Component List Virtualized Scroll manual", "[component_list]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/component_list_virtualized.riv", &silver);

    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();
    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    REQUIRE(vmi != nullptr);
    REQUIRE(stateMachine != nullptr);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    REQUIRE(artboard->find<rive::ArtboardComponentList>("List") != nullptr);

    REQUIRE(artboard->find<rive::ScrollConstraint>().size() == 1);
    REQUIRE(artboard->find<rive::ScrollConstraint>()[0] != nullptr);
    auto scroll = artboard->find<rive::ScrollConstraint>()[0];

    REQUIRE(scroll->scrollPercentY() == 0.0f);
    REQUIRE(scroll->offsetY() == 0.0f);
    REQUIRE(scroll->scrollIndex() == Approx(0.0f));
    REQUIRE(scroll->physics()->isRunning() == false);

    silver.addFrame();
    stateMachine->pointerMove(rive::Vec2D(250.0f, 50.0f));
    // Start drag
    stateMachine->pointerDown(rive::Vec2D(250.0f, 50.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    // Move left 200px in 0.1 seconds
    stateMachine->pointerMove(rive::Vec2D(50.0f, 50.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    REQUIRE(scroll->offsetX() == -200.0f);
    REQUIRE(scroll->scrollIndex() == Approx(1.818182f));

    // End drag
    stateMachine->pointerUp(rive::Vec2D(50.0f, 50.0f));

    REQUIRE(scroll->physics()->isRunning() == true);

    CHECK(silver.matches("component_list_virtualized_scroll_manual"));
}

TEST_CASE("Artboard override with horizontal distribution", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/artboard_list_overrides.riv", &silver);

    auto artboard = file->artboardNamed("Main");

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

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("artboard_list_overrides_horizontal"));
}

TEST_CASE("Artboard override with vertical distribution", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/artboard_list_overrides.riv", &silver);

    auto artboard = file->artboardNamed("Main-Vertical");

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

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("artboard_list_overrides_vertical"));
}

TEST_CASE("Number to Lists reset triggers correctly", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/reset_phase.riv", &silver);

    auto artboard = file->artboardNamed("multi-main");

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

    int frames = (int)(3.0f / 0.16f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.16f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("reset_phase_multi_main"));
}

TEST_CASE("Non Layout List Artboard Position", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/component_list_grouped.riv", &silver);

    auto artboard = file->artboardNamed("MainArtboard");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto list = artboard->find<rive::ArtboardComponentList>("List");
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.16f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.16f);
        artboard->draw(renderer.get());
    }

    auto item1 = list->listItem(0);
    auto vmInstance1 = item1->viewModelInstance();
    auto xProp1 =
        vmInstance1->propertyValue("x")->as<rive::ViewModelInstanceNumber>();
    REQUIRE(xProp1 != nullptr);
    auto xVal1 = xProp1->propertyValue();
    REQUIRE(xVal1 == 0);
    auto yProp1 =
        vmInstance1->propertyValue("y")->as<rive::ViewModelInstanceNumber>();
    REQUIRE(yProp1 != nullptr);
    auto yVal1 = yProp1->propertyValue();
    REQUIRE(yVal1 == 0);

    auto item2 = list->listItem(1);
    auto vmInstance2 = item2->viewModelInstance();
    auto xProp2 =
        vmInstance2->propertyValue("x")->as<rive::ViewModelInstanceNumber>();
    REQUIRE(xProp2 != nullptr);
    auto xVal2 = xProp2->propertyValue();
    REQUIRE(xVal2 == 50);
    auto yProp2 =
        vmInstance2->propertyValue("y")->as<rive::ViewModelInstanceNumber>();
    REQUIRE(yProp2 != nullptr);
    auto yVal2 = yProp2->propertyValue();
    REQUIRE(yVal2 == 0);

    auto item3 = list->listItem(2);
    auto vmInstance3 = item3->viewModelInstance();
    auto xProp3 =
        vmInstance3->propertyValue("x")->as<rive::ViewModelInstanceNumber>();
    REQUIRE(xProp3 != nullptr);
    auto xVal3 = xProp3->propertyValue();
    REQUIRE(xVal3 == 100);
    auto yProp3 =
        vmInstance3->propertyValue("y")->as<rive::ViewModelInstanceNumber>();
    REQUIRE(yProp3 != nullptr);
    auto yVal3 = yProp3->propertyValue();
    REQUIRE(yVal3 == 0);

    xProp1->propertyValue(-90);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();

    xProp2->propertyValue(25);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();

    xProp3->propertyValue(150);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();

    yProp1->propertyValue(-50);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();

    yProp2->propertyValue(100);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();

    yProp3->propertyValue(-200);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Hover list item 1
    for (int i = 0; i < frames; i++)
    {
        stateMachine->pointerMove(rive::Vec2D(210.0f, 250.0f));
        stateMachine->advanceAndApply(0.16f);
        artboard->draw(renderer.get());
        silver.addFrame();
    }

    // Hover list item 2
    for (int i = 0; i < frames; i++)
    {
        stateMachine->pointerMove(rive::Vec2D(325.0f, 400.0f));
        stateMachine->advanceAndApply(0.16f);
        artboard->draw(renderer.get());
        silver.addFrame();
    }

    // Hover list item 3
    for (int i = 0; i < frames; i++)
    {
        stateMachine->pointerMove(rive::Vec2D(450.0f, 100.0f));
        stateMachine->advanceAndApply(0.16f);
        artboard->draw(renderer.get());
        silver.addFrame();
    }

    CHECK(silver.matches("component_list_grouped"));
}

TEST_CASE("Artboard list with follow path constraint", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/component_list_follow_path.riv", &silver);

    auto artboard = file->artboardNamed("Main");

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

    int frames = 30;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    auto itemCountProp =
        vmi->propertyValue("ItemCount")->as<rive::ViewModelInstanceNumber>();
    REQUIRE(itemCountProp != nullptr);
    auto itemCountValue = itemCountProp->propertyValue();
    REQUIRE(itemCountValue == 10);
    // Change the list from 10 to 5 items
    itemCountProp->propertyValue(5);

    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("component_list_follow_path"));
}

TEST_CASE("Artboard list with follow path constraint distance", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/component_list_follow_path_distance.riv", &silver);

    auto artboard = file->artboardNamed("Artboard");

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

    int frames = 60;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("component_list_follow_path_distance"));
}

TEST_CASE("Component List Hit order", "[component_list]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/component_list_hit_order.riv", &silver);

    auto artboard = file->artboardNamed("MainArtboard");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();
    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    REQUIRE(vmi != nullptr);
    REQUIRE(stateMachine != nullptr);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    REQUIRE(artboard->find<rive::ArtboardComponentList>("MainList") != nullptr);

    silver.addFrame();
    // Click second item where it overlaps with first item
    stateMachine->pointerMove(rive::Vec2D(175.0f, 50.0f));
    stateMachine->pointerDown(rive::Vec2D(175.0f, 50.0f));
    stateMachine->pointerUp(rive::Vec2D(175.0f, 50.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();
    // Click third item where it overlaps with second item
    stateMachine->pointerMove(rive::Vec2D(325.0f, 50.0f));
    stateMachine->pointerDown(rive::Vec2D(325.0f, 50.0f));
    stateMachine->pointerUp(rive::Vec2D(325.0f, 50.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();
    // Click first item
    stateMachine->pointerMove(rive::Vec2D(100.0f, 50.0f));
    stateMachine->pointerDown(rive::Vec2D(100.0f, 50.0f));
    stateMachine->pointerUp(rive::Vec2D(100.0f, 50.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("component_list_hit_order"));
}

TEST_CASE("Virtualized list with nested data bound artboards",
          "[component_list]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/virtualized_artboard_databound_children.riv",
                     &silver);

    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();
    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    REQUIRE(vmi != nullptr);
    REQUIRE(stateMachine != nullptr);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    float yPos = 200.0f;

    stateMachine->pointerMove(rive::Vec2D(60.0f, yPos));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    // Start drag
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(60.0f, yPos));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    while (yPos > -500.0f)
    {
        silver.addFrame();
        stateMachine->pointerMove(rive::Vec2D(60.0f, yPos));
        yPos -= 20;
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    // End drag
    silver.addFrame();
    stateMachine->pointerUp(rive::Vec2D(60.0f, yPos));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("virtualized_artboard_databound_children"));
}

TEST_CASE("Artboard list map rules", "[component_list]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/artboard_list_map_rules.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("artboard_list_map_rules"));
}
