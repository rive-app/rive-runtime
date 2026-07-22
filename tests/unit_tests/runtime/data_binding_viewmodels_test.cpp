#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "rive/nested_artboard.hpp"
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"
#include <catch.hpp>

using namespace rive;

TEST_CASE(
    "Data bind view model to view model instance from set value, externally and from scripting",
    "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/databind_viewmodel.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    auto statefulChildVMI = file->createViewModelInstance("StatefulChild");
    auto numProp =
        statefulChildVMI->propertyValue("num")->as<ViewModelInstanceNumber>();
    numProp->propertyValue(44.0f);

    vmi->replaceViewModelByName("statefulChild", statefulChildVMI);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    numProp->propertyValue(44.0f);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    stateMachine->pointerDown(Vec2D(25.0f, 25.0f));
    stateMachine->pointerUp(Vec2D(25.0f, 25.0f));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("databind_viewmodel"));
}

TEST_CASE("Stateful component is bound before binding the view model instance",
          "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/unbound_stateful_component.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("unbound_stateful_component"));
}

TEST_CASE("Bidirectional stateful property binding", "[silver]")
{
    SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/bidirectional_stateful_property.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(175, 175));
    stateMachine->pointerUp(rive::Vec2D(175, 175));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(450, 450));
    stateMachine->pointerUp(rive::Vec2D(450, 450));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(175, 175));
    stateMachine->pointerUp(rive::Vec2D(175, 175));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(450, 450));
    stateMachine->pointerUp(rive::Vec2D(450, 450));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(450, 50));
    stateMachine->pointerUp(rive::Vec2D(450, 50));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    int frames = (int)(1.0f / 0.2f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.2f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("bidirectional_stateful_property"));
}