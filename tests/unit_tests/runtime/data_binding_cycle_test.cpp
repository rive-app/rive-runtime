#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <utils/no_op_renderer.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/color.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/text/text_value_run.hpp>
#include <rive/custom_property_number.hpp>
#include <rive/custom_property_string.hpp>
#include <rive/custom_property_boolean.hpp>
#include <rive/constraints/follow_path_constraint.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include <rive/viewmodel/viewmodel_instance_color.hpp>
#include <rive/viewmodel/viewmodel_instance_string.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_enum.hpp>
#include <rive/viewmodel/viewmodel_instance_trigger.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "rive/nested_artboard.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("view model changed by child updates on the parent on next frame",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test_3.riv");

    auto artboard = file->artboard("main-1")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto stateMachineInstance = artboard->defaultStateMachine();
    stateMachineInstance->bindViewModelInstance(viewModelInstance);
    REQUIRE(stateMachineInstance != nullptr);
    stateMachineInstance->advanceAndApply(0.0f);
    REQUIRE(artboard->find<rive::Rectangle>("sized-rect-path") != nullptr);
    auto rect = artboard->find<rive::Rectangle>("sized-rect-path");
    REQUIRE(rect->width() == 100.0f);
    // This click event is captured by a child nested artboard that updates a
    // view model value
    stateMachineInstance->pointerDown(rive::Vec2D(75.0f, 75.0f));
    stateMachineInstance->pointerUp(rive::Vec2D(75.0f, 75.0f));
    stateMachineInstance->advanceAndApply(0.0f);
    // A single advance is needed to reflect the changes on the parent affected
    // by that view model value
    REQUIRE(rect->width() == 200.0f);
}

TEST_CASE("view model changed by parent updates on the child on next frame",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test_3.riv");

    auto artboard = file->artboard("main-2")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto stateMachineInstance = artboard->defaultStateMachine();
    stateMachineInstance->bindViewModelInstance(viewModelInstance);
    REQUIRE(stateMachineInstance != nullptr);
    stateMachineInstance->advanceAndApply(0.0f);
    REQUIRE(artboard->find<rive::NestedArtboard>("child-2") != nullptr);
    auto nestedArtboardChild = artboard->find<rive::NestedArtboard>("child-2");

    auto nestedArtboardArtboardChild = nestedArtboardChild->artboardInstance();
    REQUIRE(nestedArtboardArtboardChild != nullptr);
    auto rect =
        nestedArtboardArtboardChild->find<rive::Rectangle>("child-rect-path");
    REQUIRE(rect != nullptr);
    REQUIRE(rect->width() == 100.0f);

    stateMachineInstance->pointerDown(rive::Vec2D(250.0f, 250.0f));
    stateMachineInstance->pointerUp(rive::Vec2D(250.0f, 250.0f));
    stateMachineInstance->advanceAndApply(0.0f);
    // // A single advance is needed to reflect the changes on the child
    // affected
    // // by that view model value
    REQUIRE(rect->width() == 200.0f);
}

TEST_CASE(
    "view model changed by child event updates on the parent on next frame",
    "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test_3.riv");

    auto artboard = file->artboard("main-3")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto stateMachineInstance = artboard->defaultStateMachine();
    stateMachineInstance->bindViewModelInstance(viewModelInstance);
    REQUIRE(stateMachineInstance != nullptr);
    stateMachineInstance->advanceAndApply(0.0f);
    REQUIRE(artboard->find<rive::Rectangle>("sized-rect-path") != nullptr);
    auto rect = artboard->find<rive::Rectangle>("sized-rect-path");
    REQUIRE(rect->width() == 100.0f);
    // An event is triggered at 0.5s that will perform a change on the view
    // model
    stateMachineInstance->advanceAndApply(0.5f);
    REQUIRE(rect->width() == 100.0f);
    // An extra advance is needed for the change to be propagated
    stateMachineInstance->advanceAndApply(0.0f);
    REQUIRE(rect->width() == 200.0f);
}

TEST_CASE(
    "view model changed by parent event updates on the child on next frame",
    "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test_3.riv");

    auto artboard = file->artboard("main-4")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto stateMachineInstance = artboard->defaultStateMachine();
    stateMachineInstance->bindViewModelInstance(viewModelInstance);
    REQUIRE(stateMachineInstance != nullptr);
    stateMachineInstance->advanceAndApply(0.0f);
    REQUIRE(artboard->find<rive::NestedArtboard>("child-4") != nullptr);
    auto nestedArtboardChild = artboard->find<rive::NestedArtboard>("child-4");

    auto nestedArtboardArtboardChild = nestedArtboardChild->artboardInstance();
    REQUIRE(nestedArtboardArtboardChild != nullptr);
    auto rect =
        nestedArtboardArtboardChild->find<rive::Rectangle>("child-rect-path");
    REQUIRE(rect != nullptr);
    REQUIRE(rect->width() == 100.0f);
    // An event on the parent triggers a view model change
    stateMachineInstance->advanceAndApply(0.5f);
    REQUIRE(rect->width() == 100.0f);
    // An extra advance is needed for the change to be propagated
    stateMachineInstance->advanceAndApply(0.0f);
    REQUIRE(rect->width() == 200.0f);
}

TEST_CASE("view model changed by child target to source prop changes on the "
          "same frame on parent",
          "[data binding]")
{

    auto file = ReadRiveFile("assets/data_binding_test_3.riv");

    auto artboard = file->artboard("main-5")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto stateMachineInstance = artboard->defaultStateMachine();
    stateMachineInstance->bindViewModelInstance(viewModelInstance);
    REQUIRE(stateMachineInstance != nullptr);
    stateMachineInstance->advanceAndApply(0.0f);
    REQUIRE(artboard->find<rive::TextValueRun>("text-run-test") != nullptr);
    auto textRunChild = artboard->find<rive::TextValueRun>("text-run-test");
    REQUIRE(textRunChild->text() == "before");
    stateMachineInstance->advanceAndApply(0.5f);
    REQUIRE(textRunChild->text() == "after");
}

TEST_CASE("view model changed by parent target to source prop changes on the "
          "same frame on child",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test_3.riv");

    auto artboard = file->artboard("main-6")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto stateMachineInstance = artboard->defaultStateMachine();
    stateMachineInstance->bindViewModelInstance(viewModelInstance);
    REQUIRE(stateMachineInstance != nullptr);
    stateMachineInstance->advanceAndApply(0.0f);
    REQUIRE(artboard->find<rive::NestedArtboard>("child-6") != nullptr);
    auto nestedArtboardChild = artboard->find<rive::NestedArtboard>("child-6");

    auto nestedArtboardArtboardChild = nestedArtboardChild->artboardInstance();
    REQUIRE(nestedArtboardArtboardChild != nullptr);
    auto textRunParent =
        nestedArtboardArtboardChild->find<rive::TextValueRun>("child-text-run");
    REQUIRE(textRunParent != nullptr);
    REQUIRE(textRunParent->text() == "parent-before");
    stateMachineInstance->advanceAndApply(0.5f);
    REQUIRE(textRunParent->text() == "parent-after");
}

TEST_CASE("view model changed by three artboard levels", "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test_3.riv");

    auto artboard = file->artboard("main-7")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto stateMachineInstance = artboard->defaultStateMachine();
    stateMachineInstance->bindViewModelInstance(viewModelInstance);
    REQUIRE(stateMachineInstance != nullptr);
    stateMachineInstance->advanceAndApply(0.0f);
    REQUIRE(artboard->find<rive::TextValueRun>("main-run") != nullptr);
    auto mainRun = artboard->find<rive::TextValueRun>("main-run");
    REQUIRE(artboard->find<rive::NestedArtboard>("child-7") != nullptr);
    auto nestedArtboardChild = artboard->find<rive::NestedArtboard>("child-7");

    auto nestedArtboardArtboardChild = nestedArtboardChild->artboardInstance();
    REQUIRE(nestedArtboardArtboardChild != nullptr);
    auto childRun =
        nestedArtboardArtboardChild->find<rive::TextValueRun>("child-run");
    REQUIRE(childRun != nullptr);
    REQUIRE(nestedArtboardArtboardChild->find<rive::NestedArtboard>(
                "grand-child-7") != nullptr);
    auto nestedArtboardGrandChild =
        nestedArtboardArtboardChild->find<rive::NestedArtboard>(
            "grand-child-7");
    auto nestedArtboardGrandArtboardChild =
        nestedArtboardGrandChild->artboardInstance();
    auto grandChildRun =
        nestedArtboardGrandArtboardChild->find<rive::TextValueRun>(
            "grand-child-run");
    REQUIRE(grandChildRun != nullptr);

    stateMachineInstance->advanceAndApply(0.5f);
    REQUIRE(mainRun->text() == "main-test-2");
    REQUIRE(childRun->text() == "main-test-2");
    REQUIRE(grandChildRun->text() == "main-test-2");

    stateMachineInstance->advanceAndApply(1.5f);
    REQUIRE(mainRun->text() == "child-text-1");
    REQUIRE(childRun->text() == "child-text-1");
    REQUIRE(grandChildRun->text() == "child-text-1");

    stateMachineInstance->advanceAndApply(0.5f);
    REQUIRE(mainRun->text() == "child-text-2");
    REQUIRE(childRun->text() == "child-text-2");
    REQUIRE(grandChildRun->text() == "child-text-2");

    stateMachineInstance->advanceAndApply(1.5f);
    REQUIRE(mainRun->text() == "grand-child-text-1");
    REQUIRE(childRun->text() == "grand-child-text-1");
    REQUIRE(grandChildRun->text() == "grand-child-text-1");

    stateMachineInstance->advanceAndApply(.5f);
    REQUIRE(mainRun->text() == "grand-child-text-2");
    REQUIRE(childRun->text() == "grand-child-text-2");
    REQUIRE(grandChildRun->text() == "grand-child-text-2");
}